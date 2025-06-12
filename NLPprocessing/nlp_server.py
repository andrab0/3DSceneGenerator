from flask import Flask, request, jsonify
import requests

app = Flask(__name__)
NLP_SERVER_URL = "http://localhost:5001/process_nlp"

def extract_objects_and_relations(text):
    """Extrage obiecte si relatii folosind datele NLP de la worker."""
    response = requests.post(NLP_SERVER_URL, json={"text": text})
    if response.status_code != 200:
        return {"error": "NLP processing failed"}

    data = response.json()
    objects = []
    object_attributes = {}
    relations = []

    for sentence in data["sentences"]:
        words = sentence["words"]
        print(f"\n📌 Processing sentence: {sentence['text']}")

        # Pas 1: Detectam relatiile multi-cuvant
        multiword_relations = []
        for i in range(len(words) - 2):
            if (words[i]["upos"] == "ADP" and
                words[i+1]["upos"] == "NOUN" and
                words[i+2]["upos"] == "ADP"):
                relation = {
                    "phrase": " ".join([words[i]["text"], words[i+1]["text"], words[i+2]["text"]]),
                    "start": i,
                    "end": i+2
                }
                multiword_relations.append(relation)
                print(f"🔍 Detected multi-word relation: {relation['phrase']}")

        # Pas 2: Identificam cuvintele dependente
        excluded_nouns = set()
        for rel in multiword_relations:
            for i in range(rel["start"], rel["end"] + 1):
                if words[i]["upos"] == "NOUN":
                    excluded_nouns.add(words[i]["text"])

        # Pas 3: Extragem obiectele principale
        for word in words:
            if word["upos"] == "NOUN" and word["text"] not in excluded_nouns:
                if word["text"] not in objects:
                    objects.append(word["text"])
                    object_attributes[word["text"]] = {"color": [], "size": []}
                    print(f"✅ New object: {word['text']}")

        # Pas 4: Atribuim adjectivele
        for word in words:
            if word["upos"] == "ADJ":
                head_id = word["head"]
                if head_id > 0 and head_id <= len(words):
                    head_word = words[head_id - 1]
                    if head_word["upos"] == "NOUN" and head_word["text"] in objects:
                        if word["text"] in ["small", "large"]:
                            object_attributes[head_word["text"]]["size"].append(word["text"])
                        else:
                            object_attributes[head_word["text"]]["color"].append(word["text"])
                        print(f"📥 Added attribute '{word['text']}' to {head_word['text']}")

        # Pas 5: Procesam relatiile
        for rel in multiword_relations:
            middle_word = words[rel["start"] + 1]
            head_word = words[middle_word["head"] - 1] if middle_word["head"] > 0 else None
            last_word = words[rel["end"]]
            target_word = words[last_word["head"] - 1] if last_word["head"] > 0 else None

            if head_word and target_word and head_word["text"] in objects and target_word["text"] in objects:
                relations.append({
                    "object_1": head_word["text"],
                    "relation": rel["phrase"],
                    "object_2": target_word["text"]
                })
                print(f"✅ Detected relation: {head_word['text']} -- {rel['phrase']} -- {target_word['text']}")

    # Curatare atribute finale
    for obj in object_attributes:
        object_attributes[obj]["color"] = " ".join(object_attributes[obj]["color"]) or None
        object_attributes[obj]["size"] = " ".join(object_attributes[obj]["size"]) or None

    return {
        "objects": [{"object": obj, "attributes": object_attributes[obj]} for obj in objects],
        "relations": relations
    }

@app.route("/process", methods=["POST"])
def process_text():
    """Endpoint principal pentru procesarea textului."""
    data = request.json
    text = data.get("text", "")

    if not text:
        return jsonify({"error": "No text provided"}), 400

    result = extract_objects_and_relations(text)
    return jsonify(result)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)