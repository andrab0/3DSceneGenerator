from flask import Flask, request, jsonify
import stanza

app = Flask(__name__)

print("🔄 [INFO] Loading Stanza NLP model...")
nlp = stanza.Pipeline("en", processors="tokenize,mwt,pos,lemma,depparse", verbose=False)
print("✅ [DONE] Model loaded.")

@app.route("/process_nlp", methods=["POST"])
def process_nlp():
    """Procesează textul și returnează structura linguistică detaliată."""
    data = request.json
    text = data.get("text", "")

    if not text:
        return jsonify({"error": "No text provided"}), 400

    doc = nlp(text)
    result = {"sentences": []}

    for sent in doc.sentences:
        sentence_data = {
            "text": sent.text,
            "words": [{
                "id": w.id,
                "text": w.text,
                "upos": w.upos,
                "head": w.head,
                "deprel": w.deprel
            } for w in sent.words]
        }
        result["sentences"].append(sentence_data)

    return jsonify(result)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001, debug=False)