from flask import Flask, request, jsonify
import threading
import spacy
from transformers import pipeline, MarianMTModel, MarianTokenizer
from sentence_transformers import SentenceTransformer, util

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)
warnings.filterwarnings("ignore", category=UserWarning)
import tensorflow as tf
tf.get_logger().setLevel('ERROR')

app = Flask(__name__)

# === GLOBALS ===
models_ready = False
spacy_en = None
rebel = None
embedder = None
RELATION_EMBEDDINGS = None

# Supported languages
SUPPORTED_LANGUAGES = {
    "en": "English",
    "ro": "Romanian",
    "fr": "French",
    "de": "German",
    "es": "Spanish",
    "it": "Italian"
}

# Spatial relations
RELATION_LABELS = {
    "left_of": "left of",
    "right_of": "right of",
    "in_front_of": "in front of",
    "behind": "behind",
    "above": "above",
    "below": "below",
    "under": "under",
    "on": "on",
    "next_to": "next to",
    "between": "between",
    "near": "near",
    "inside": "inside",
    "on_top_of": "on top of"
}

# Spatial relations (priority for conflict resolution)
RELATION_PRIORITY = {
    "on_top_of": 3,
    "on": 2,
    "above": 2,
    "under": 2,
    "below": 2,
    "inside": 2,
    "next_to": 2,
    "near": 1,
    "in_front_of": 1,
    "behind": 1,
    "left_of": 1,
    "right_of": 1,
    "between": 1
}

RELATION_TEXTS = list(RELATION_LABELS.values())
RELATION_KEYS = list(RELATION_LABELS.keys())

# Translator cache
loaded_translators = {}

def preloadModelsLazyLoad():
    """Preload models in a separate thread."""
    global spacy_en, rebel, embedder, RELATION_EMBEDDINGS, models_ready

    print("🔄 Loading models in background...")
    spacy_en = spacy.load("en_core_web_sm")
    rebel = pipeline("text2text-generation", model="Babelscape/rebel-large")
    embedder = SentenceTransformer("sentence-transformers/all-MiniLM-L6-v2")
    RELATION_EMBEDDINGS = embedder.encode(RELATION_TEXTS, convert_to_tensor=True)

    models_ready = True
    print("✅ All models are ready.")


def translateToEnglish(text, lang):
    """Translate text to English using MarianMT."""
    if lang == "en":
        return text

    MODEL_MAP = {
        "ro": "Helsinki-NLP/opus-mt-ROMANCE-en",
        "fr": "Helsinki-NLP/opus-mt-ROMANCE-en",
        "es": "Helsinki-NLP/opus-mt-ROMANCE-en",
        "it": "Helsinki-NLP/opus-mt-ROMANCE-en"
    }

    model_name = MODEL_MAP.get(lang)
    if not model_name:
        raise ValueError(f"Unsupported language '{lang}' for translation.")

    if model_name not in loaded_translators:
        tokenizer = MarianTokenizer.from_pretrained(model_name)
        model = MarianMTModel.from_pretrained(model_name)
        loaded_translators[model_name] = (tokenizer, model)

    tokenizer, model = loaded_translators[model_name]
    inputs = tokenizer(text, return_tensors="pt", padding=True, truncation=True)
    outputs = model.generate(**inputs)
    return tokenizer.decode(outputs[0], skip_special_tokens=True)

def normalizeRelationDynamic(raw):
    """Normalize the relation text to match the predefined labels."""
    if not raw:
        return None

    # Embed the raw relation text - not case-sensitive
    raw_embedding = embedder.encode(raw.lower(), convert_to_tensor=True)

    # Compute cosine similarities with predefined relations
    scores = util.cos_sim(raw_embedding, RELATION_EMBEDDINGS)[0]
    # similarities = util.pytorch_cos_sim(raw_em/bedding, RELATION_EMBEDDINGS)[0]

    # Find the index of the most similar relation
    best_match_index = int (scores.argmax())
    # max_index = similarities.argmax().item()

    # Return the corresponding relation label
    return RELATION_KEYS[best_match_index] if scores[best_match_index] > 0.7 else None
    # return RELATION_KEYS[max_index] if similarities[max_index] > 0.7 else None

def extractObjectsAndAttributes(doc):
    """Extract objects and their attributes (color, size) from the document."""

    IGNORED_OBJECTS = {"top", "bottom", "middle", "side", "front", "brack", "surface"}

    objects = {}
    # Prima trecere a tuturor chunk-urilor nominale
    for chunk in doc.noun_chunks:
        noun = chunk.root.lemma_.lower()
        if noun in IGNORED_OBJECTS:
            continue
        adjectives = [tok.text.lower() for tok in chunk if tok.pos_ == "ADJ"]
        if noun not in objects:
            objects[noun] = {"color": [], "size": []}
        for adj in adjectives:
            if adj in ["red", "blue", "green", "black", "white", "pink", "brown", "yellow", "wooden", "metal", "glass", "various"]:
                objects[noun]["color"].append(adj)
            elif adj in ["small", "large", "tall", "short", "multiple", "fresh"]:
                objects[noun]["size"].append(adj)
            else:
                objects[noun]["color"].append(adj)

    # A doua trecere: adauga nouns care nu sunt in chunk-uri nominale
    for token in doc:
        if token.pos_ != "NOUN":
            continue

        noun = token.lemma_.lower()
        if noun in IGNORED_OBJECTS or noun in objects:
            continue

        is_valid_dep = token.dep_ in {"dobj", "pobj", "attr", "nsubj", "conj", "obl", "compound"}
        is_attached_to_verb = token.head.pos_ in {"VERB", "AUX"}

        if is_valid_dep or is_attached_to_verb:
            objects[noun] = {"color": [], "size": []}

            # Adjective copil
            for child in token.children:
                if child.pos_ == "ADJ":
                    adj = child.text.lower()
                    if adj in ["red", "blue", "green", "black", "white", "pink", "brown", "yellow", "wooden", "metal", "glass", "various"]:
                        objects[noun]["color"].append(adj)
                    elif adj in ["small", "large", "tall", "short", "multiple", "fresh"]:
                        objects[noun]["size"].append(adj)
                    else:
                        objects[noun]["color"].append(adj)

            # Adjective head
            if token.head.pos_ == "ADJ":
                adj = token.head.text.lower()
                if adj in ["red", "blue", "green", "black", "white", "pink", "brown", "yellow", "wooden", "metal", "glass", "various"]:
                    objects[noun]["color"].append(adj)
                elif adj in ["small", "large", "tall", "short", "multiple", "fresh"]:
                    objects[noun]["size"].append(adj)
                else:
                    objects[noun]["color"].append(adj)


    return objects

def extractSpatialRelations(text):
    """Extract spatial relations from the text using REBEL."""
    results = rebel(text)
    relations = []

    for r in results:
        parts = r["generated_text"].split("|")
        if len(parts) != 3:
            continue
        subj, raw_rel, obj = [p.strip().lower() for p in parts]
        norm_rel = normalizeRelationDynamic(raw_rel)
        if subj and obj and norm_rel:
            relations.append({
                "object_1": subj,
                "relation": norm_rel,
                "object_2": obj
            })

    return relations
    # for result in results:
    #     generated_text = result["generated_text"]
    #     if "|" in generated_text:
    #         parts = generated_text.split("|")
    #         if len(parts) == 3:
    #             subject, relation, obj = parts
    #             normalized_relation = normalizeRelationDynamic(relation.strip())
    #             if normalized_relation:
    #                 relations.append({
    #                     "object_1": subject.strip(),
    #                     "relation": normalized_relation,
    #                     "object_2": obj.strip()
    #                 })

    # return relations

def extractSpatialRelationsFallback(doc, knownObjects):
    """Fallback rule-based extractor for spatial relations if REBEL fails."""
    spatialKeywords = {
        "on top of": "on_top_of",
        "on": "on",
        "under": "under",
        "above": "above",
        "below": "below",
        "next to": "next_to",
        "behind": "behind",
        "in front of": "in_front_of",
        "near": "near",
        "beside": "next_to",
        "inside": "inside"
    }

    text = doc.text.lower()
    relations = []

    for phrase, relation in spatialKeywords.items():
        if phrase in text:
            parts = text.split(phrase)
            if len(parts) == 2:
                subjDoc = spacy_en(parts[0])
                objDoc = spacy_en(parts[1])

                subj = [token.lemma_ for token in subjDoc if token.lemma_ in knownObjects]
                obj = [token.lemma_ for token in objDoc if token.lemma_ in knownObjects]

                if subj and obj:
                    relations.append({
                        "object_1": subj[-1],
                        "relation": relation,
                        "object_2": obj[0]
                    })

    return relations


@app.route('/process', methods=['POST'])
def processText():
    """Endpoint for processing text."""

    if not models_ready:
        return jsonify({"error": "Models not loaded yet. Try again shortly."}), 503
    data = request.json
    text = data.get("text", "")
    lang = data.get("lang", "en").lower()

    if not text:
        return jsonify({"error": "No text provided"}), 400

    if lang not in SUPPORTED_LANGUAGES:
        return jsonify({
            "error": f"Unsupported language '{lang}'. Supported: {', '.join(SUPPORTED_LANGUAGES.keys())}"
        }), 400

    try:
        translated = translateToEnglish(text, lang)
        print(f"Translated [{lang}] → EN: {translated}")
    except Exception as e:
        return jsonify({"error": f"Translation failed: {str(e)}"}), 500

    doc = spacy_en(translated)
    raw_objects = extractObjectsAndAttributes(doc)
    relations = extractSpatialRelations(translated)
    if not relations:
        relations = extractSpatialRelationsFallback(doc, raw_objects.keys())

    # Genereaza IDs unice + include `type`
    objects = []
    object_counts = {}
    for obj, attrs in raw_objects.items():
        count = object_counts.get(obj, 0) + 1
        object_counts[obj] = count
        obj_id = f"{obj}_{count}"
        objects.append({
            "id": obj_id,
            "type": obj,
            "attributes": {
                "color": " ".join(attrs["color"]) or None,
                "size": " ".join(attrs["size"]) or None
            }
        })

    # Normalizeaza relatiile pe baza ID-urilor
    id_map = {o["type"]: o["id"] for o in objects}
    normalized_relations = {}
    for rel in relations:
        o1 = id_map.get(rel["object_1"])
        o2 = id_map.get(rel["object_2"])
        relation = rel["relation"]
        if not o1 or not o2 or not relation:
            continue

        key = (o1, o2)
        currentPriority = RELATION_PRIORITY.get(relation, 0)

        if key in normalized_relations:
            existing = normalized_relations[key]
            existing_priority = RELATION_PRIORITY.get(existing["relation"], 0)
            if currentPriority > existing_priority:
                normalized_relations[key] = {
                    "object_1": o1,
                    "relation": relation,
                    "object_2": o2
                }
        else:
            normalized_relations[key] = {
                "object_1": o1,
                "relation": relation,
                "object_2": o2
            }

    unique_relations = list(normalized_relations.values())

    return jsonify({
        "objects": objects,
        "relations": unique_relations
    })

if __name__ == "__main__":
    print("🌐 Starting NLP Processing Service...")
    threading.Thread(target=preloadModelsLazyLoad, daemon=True).start()
    print("🚀 Starting Flask on port 5000...")
    app.run(host="0.0.0.0", port=5000)