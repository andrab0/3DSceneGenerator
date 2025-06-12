# implement simple FLASK server with POST /process endpoint that prints the request body
from flask import Flask, request, jsonify
app = Flask(__name__)
@app.route('/process', methods=['POST'])
def process():
    data = request.get_json()
    if not data:
        return jsonify({"error": "No data provided"}), 400
    print("Received data:", data)
    return jsonify({"message": "Data received successfully"}), 200
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
