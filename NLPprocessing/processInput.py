from flask import Flask, request, jsonify
import stanza
from transformers import pipeline
from sentence_transformers import SentenceTransformer, util
import os

# 🔹 Inițializare server Flask
app = Flask(__name__)

# 🔹 Inițializare modele NLP
print("🔄 [INFO] Initializing NLP models...")

# 1. Stanza pentru analiza sintactică
stanza.download("en", processors="tokenize,mwt,pos,lemma,depparse", verbose=False)
nlp = stanza.Pipeline("en", processors="tokenize,mwt,pos,lemma,depparse", verbose=False)

# 2. Transformers pentru extragerea relațiilor
relation_extractor = pipeline("text2text-generation", model="Babelscape/rebel-large")

# 3. SentenceTransformer pentru compararea relațiilor spațiale
relation_model = SentenceTransformer("sentence-transformers/all-MiniLM-L6-v2")

print("✅ [DONE] Models loaded. Server ready to process text.")

# 🔹 Expresii de relații spațiale comune
SPATIAL_RELATIONS = ["left of", "right of", "in front of", "behind", "above", "below"]

def extract_objects_and_attributes(doc):
    """Extrage obiectele și atributele (culoare, dimensiune) din text."""
    objects = []
    object_attributes = {}

    for sentence in doc.sentences:
        # Parcurgem fiecare cuvânt din propoziție
        for word in sentence.words:
            if word.upos == "NOUN":  # Identifică substantive (obiecte)
                obj_name = word.text
                objects.append(obj_name)
                object_attributes[obj_name] = {"color": None, "size": None}

                # Căutăm adjective asociate cu substantivul curent
                for child in sentence.words:
                    if child.head == word.id and child.upos == "ADJ":  # Adjective dependente de substantiv
                        if object_attributes[obj_name]["color"] is None:
                            object_attributes[obj_name]["color"] = child.text
                        else:
                            object_attributes[obj_name]["size"] = child.text

    return objects, object_attributes

def extract_spatial_relations(text):
    """Extrage relațiile spațiale folosind un model de deep learning."""
    relations = []

    # Folosim modelul REBEL pentru extragerea relațiilor
    rebel_results = relation_extractor(text)
    for result in rebel_results:
        generated_text = result["generated_text"]
        # Parsăm rezultatul pentru a extrage subiect, relație și obiect
        if "|" in generated_text:
            parts = generated_text.split("|")
            if len(parts) == 3:
                subject, relation, obj = parts
                # Filtrăm doar relațiile spațiale
                if any(spatial_rel in relation.lower() for spatial_rel in SPATIAL_RELATIONS):
                    relations.append({
                        "object_1": subject.strip(),
                        "relation": relation.strip(),
                        "object_2": obj.strip()
                    })

    return relations

def resolve_spatial_relations(objects, relations):
    """Rezolvă relațiile spațiale pentru a determina pozițiile obiectelor."""
    resolved_relations = []

    for relation in relations:
        obj1 = relation["object_1"]
        obj2 = relation["object_2"]
        rel = relation["relation"]

        # Verificăm dacă obiectele există în listă
        if obj1 in objects and obj2 in objects:
            resolved_relations.append({
                "object_1": obj1,
                "relation": rel,
                "object_2": obj2
            })

    return resolved_relations

@app.route("/process", methods=["POST"])
def process_text():
    """Endpoint pentru procesarea NLP a textului."""
    data = request.json
    text = data.get("text", "")

    if not text:
        return jsonify({"error": "No text provided"}), 400

    # Analizăm textul folosind Stanza
    doc = nlp(text)

    # Extragem obiectele și atributele
    objects, object_attributes = extract_objects_and_attributes(doc)

    # Extragem relațiile spațiale folosind modelul REBEL
    relations = extract_spatial_relations(text)

    # Rezolvăm relațiile spațiale
    resolved_relations = resolve_spatial_relations(objects, relations)

    # Construim răspunsul JSON
    scene_data = {
        "objects": [{"object": obj, "attributes": object_attributes[obj]} for obj in objects],
        "relations": resolved_relations
    }

    return jsonify(scene_data)

if __name__ == "__main__":
    # Pornim serverul Flask
    app.run(host="0.0.0.0", port=5000, debug=False)
