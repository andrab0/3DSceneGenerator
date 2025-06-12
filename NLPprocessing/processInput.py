from flask import Flask, request, jsonify
import stanza
from transformers import pipeline
from sentence_transformers import SentenceTransformer, util
import os

# 🔹 Initializare server Flask
app = Flask(__name__)

# 🔹 Initializare modele NLP
print("🔄 [INFO] Initializing NLP models...")

# 1. Stanza pentru analiza sintactica
stanza.download("en", processors="tokenize,mwt,pos,lemma,depparse", verbose=False)
nlp = stanza.Pipeline("en", processors="tokenize,mwt,pos,lemma,depparse", verbose=False)

# 2. Transformers pentru extragerea relatiilor
relation_extractor = pipeline("text2text-generation", model="Babelscape/rebel-large")

# 3. SentenceTransformer pentru compararea relatiilor spatiale
relation_model = SentenceTransformer("sentence-transformers/all-MiniLM-L6-v2")

print("✅ [DONE] Models loaded. Server ready to process text.")

# 🔹 Expresii de relatii spatiale comune
SPATIAL_RELATIONS = ["left of", "right of", "in front of", "behind", "above", "below"]

def extract_objects_and_attributes(doc):
    """Extrage obiectele si atributele (culoare, dimensiune) din text."""
    objects = []
    object_attributes = {}

    for sentence in doc.sentences:
        # Parcurgem fiecare cuvant din propozitie
        for word in sentence.words:
            if word.upos == "NOUN":  # Identifica substantive (obiecte)
                obj_name = word.text
                objects.append(obj_name)
                object_attributes[obj_name] = {"color": None, "size": None}

                # Cautam adjective asociate cu substantivul curent
                for child in sentence.words:
                    if child.head == word.id and child.upos == "ADJ":  # Adjective dependente de substantiv
                        if object_attributes[obj_name]["color"] is None:
                            object_attributes[obj_name]["color"] = child.text
                        else:
                            object_attributes[obj_name]["size"] = child.text

    return objects, object_attributes

def extract_spatial_relations(text):
    """Extrage relatiile spatiale folosind un model de deep learning."""
    relations = []

    # Folosim modelul REBEL pentru extragerea relatiilor
    rebel_results = relation_extractor(text)
    for result in rebel_results:
        generated_text = result["generated_text"]
        # Parsam rezultatul pentru a extrage subiect, relatie si obiect
        if "|" in generated_text:
            parts = generated_text.split("|")
            if len(parts) == 3:
                subject, relation, obj = parts
                # Filtram doar relatiile spatiale
                if any(spatial_rel in relation.lower() for spatial_rel in SPATIAL_RELATIONS):
                    relations.append({
                        "object_1": subject.strip(),
                        "relation": relation.strip(),
                        "object_2": obj.strip()
                    })

    return relations

def resolve_spatial_relations(objects, relations):
    """Rezolva relatiile spatiale pentru a determina pozitiile obiectelor."""
    resolved_relations = []

    for relation in relations:
        obj1 = relation["object_1"]
        obj2 = relation["object_2"]
        rel = relation["relation"]

        # Verificam daca obiectele exista in lista
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

    # Analizam textul folosind Stanza
    doc = nlp(text)

    # Extragem obiectele si atributele
    objects, object_attributes = extract_objects_and_attributes(doc)

    # Extragem relatiile spatiale folosind modelul REBEL
    relations = extract_spatial_relations(text)

    # Rezolvam relatiile spatiale
    resolved_relations = resolve_spatial_relations(objects, relations)

    # Construim raspunsul JSON
    scene_data = {
        "objects": [{"object": obj, "attributes": object_attributes[obj]} for obj in objects],
        "relations": resolved_relations
    }

    return jsonify(scene_data)

if __name__ == "__main__":
    # Pornim serverul Flask
    app.run(host="0.0.0.0", port=5000, debug=False)
