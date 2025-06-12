import json
from openai import OpenAI  # New import style
from typing import Dict, List, Optional, Any
from flask import Flask, request, jsonify
from dataclasses import dataclass
import logging
from datetime import datetime
import re
import os

# Configure logging - Only essential info
logging.basicConfig(
    level=logging.INFO,
    format='%(levelname)s: %(message)s',  # Simplified format
    handlers=[logging.StreamHandler()]
)
logger = logging.getLogger(__name__)

# Disable werkzeug (Flask) logging
log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)  # Only show errors, not requests

app = Flask(__name__)

# Configure OpenAI with proper validation (NEW API)
api_key = os.getenv('OPENAI_API_KEY')
if not api_key:
    logger.error("OPENAI_API_KEY environment variable not set!")
    raise ValueError("Please set OPENAI_API_KEY environment variable with your OpenAI API key")

client = OpenAI(api_key=api_key)  # New client initialization

@dataclass
class ProcessingStats:
    """Track processing statistics and costs."""
    input_tokens: int
    output_tokens: int
    processing_time: float
    cost: float
    model_used: str

class FixedSceneProcessor:
    """Fixed OpenAI-based processor with simplified, effective prompts."""

    def __init__(self):
        # Cost per 1M tokens for OpenAI models
        self.model_costs = {
            "gpt-4o": {"input": 2.50e-6, "output": 10.00e-6},
            "gpt-4o-mini": {"input": 0.15e-6, "output": 0.60e-6},
            "gpt-4": {"input": 30.00e-6, "output": 60.00e-6},
            "gpt-3.5-turbo": {"input": 0.50e-6, "output": 1.50e-6}
        }

        # Use gpt-3.5-turbo to save costs (change back to "gpt-4o" when quota available)
        self.default_model = "gpt-3.5-turbo"  # Cheaper model for testing
        self.stats = []

    def create_system_prompt(self) -> str:
        """Create a much simpler and more direct system prompt."""

        return """Extract objects, spatial relationships, and animations from 3D scene descriptions.

OUTPUT ONLY VALID JSON in this EXACT format:
{
    "objects": [
        {"id": "object_type_number", "object": "object_name", "attributes": {"color": "color_or_null", "size": "size_or_null", "animations": ["animation"] or null}}
    ],
    "relations": [
        {"object_1": "id1", "relation": "spatial_relation", "object_2": "id2"}
    ],
    "animation_couples": [
        {"primary_object": "id1", "primary_animation": "orbital", "reference_object": "id2", "description": "brief_desc"}
    ]
}

RULES:
1. Extract ONLY physical objects, preserving exact names including compound names with underscores (painted_wooden_sofa, painted_wooden_cabinet2, chinese_stool, etc.)
2. Skip abstract words (center, room, triangle, scene, etc.)
3. Give each object unique ID based on object name: painted_wooden_sofa_1, painted_wooden_sofa_2, chinese_stool_1, etc.
4. Detect colors: red, blue, green, white, black, etc.
5. Detect sizes: large, small, medium, tall, etc.
6. Detect animations: rotate, bounce, float, glow, pulse, etc.
7. Use relations: on, under, near, above, below, inside, next_to, between, left_of, right_of
8. For "X rotates around Y" use animation_couples with "orbital"
9. Preserve object names EXACTLY as written in the input text (case sensitive)
10. Do NOT invent objects, colors, or relations not explicitly mentioned in the text
11. For basic shapes (sphere, cube, cone), use the base object name and put color in attributes:
- 'golden sphere' → object: 'sphere', attributes: {'color': 'golden'}
- 'silver cube' → object: 'cube', attributes: {'color': 'silver'}
12. Only create relations between actual physical objects, not abstract concepts like 'entrance', 'furniture', 'wall'.

Extract ONLY what is explicitly mentioned. Do NOT add extra objects."""

    def create_user_prompt(self, text: str) -> str:
        """Create simple user prompt."""
        return f"Extract objects and relationships from this scene:\n\n{text}\n\nOutput JSON only:"

    def call_openai_api(self, text: str, language: str) -> Dict[str, Any]:
        """Make OpenAI API call with simplified approach."""

        start_time = datetime.now()

        # Always use gpt-4o for complex scenes
        model = "gpt-4o"

        # Create simplified prompts
        system_prompt = self.create_system_prompt()
        user_prompt = self.create_user_prompt(text)

        try:
            # NEW OPENAI API SYNTAX
            response = client.chat.completions.create(
                model=model,
                messages=[
                    {"role": "system", "content": system_prompt},
                    {"role": "user", "content": user_prompt}
                ],
                temperature=0.0,  # Zero temperature for maximum consistency
                max_tokens=2000,  # More tokens for complex scenes
                top_p=1.0
            )

            # Extract response data
            response_text = response.choices[0].message.content.strip()

            # Calculate costs and metrics
            usage = response.usage
            input_tokens = usage.prompt_tokens
            output_tokens = usage.completion_tokens

            model_cost = self.model_costs[model]
            total_cost = (input_tokens * model_cost["input"]) + (output_tokens * model_cost["output"])

            processing_time = (datetime.now() - start_time).total_seconds()

            # Track statistics
            stats = ProcessingStats(
                input_tokens=input_tokens,
                output_tokens=output_tokens,
                processing_time=processing_time,
                cost=total_cost,
                model_used=model
            )
            self.stats.append(stats)

            return {
                "response": response_text,
                "stats": {
                    "model_used": model,
                    "input_tokens": input_tokens,
                    "output_tokens": output_tokens,
                    "cost_usd": round(total_cost, 6),
                    "processing_time": round(processing_time, 2)
                }
            }

        except Exception as e:
            logger.error(f"OpenAI API call failed: {str(e)}")
            raise Exception(f"OpenAI API error: {str(e)}")

    def parse_and_validate_response(self, response_text: str) -> Dict[str, Any]:
        """Parse and validate the JSON response from OpenAI."""

        try:
            # Try direct JSON parsing
            result = json.loads(response_text)

        except json.JSONDecodeError:
            # Fallback: extract JSON from response
            json_match = re.search(r'\{.*\}', response_text, re.DOTALL)
            if json_match:
                try:
                    result = json.loads(json_match.group())
                except json.JSONDecodeError as e:
                    logger.error(f"JSON parse failed: {str(e)}")
                    raise Exception(f"Failed to parse extracted JSON: {str(e)}")
            else:
                logger.error("No JSON found in response")
                raise Exception("No valid JSON found in OpenAI response")

        # Validate structure
        if not isinstance(result, dict):
            raise Exception("Response is not a JSON object")

        if "objects" not in result:
            raise Exception("Missing required key 'objects'")

        if not isinstance(result["objects"], list):
            raise Exception("'objects' must be an array")

        # Ensure required fields exist
        if "relations" not in result:
            result["relations"] = []
        if "animation_couples" not in result:
            result["animation_couples"] = []

        return result

    def save_scene_to_file(self, scene_data: Dict[str, Any]) -> None:
        """Save scene data to scene_output.json in the temp folder."""
        # Navigate from NLPprocessing folder to project root, then to temp folder
        # Current script is in: prj/NLPprocessing/processLLM.py
        # Target folder is: prj/temp/
        current_dir = os.path.dirname(os.path.abspath(__file__))  # NLPprocessing folder
        project_root = os.path.dirname(current_dir)  # prj folder
        temp_dir = os.path.join(project_root, "temp")

        # Create temp directory if it doesn't exist
        os.makedirs(temp_dir, exist_ok=True)

        # Fixed filename
        filepath = os.path.join(temp_dir, "scene_output.json")

        # Save only the core scene data (objects, relations, animation_couples)
        core_data = {
            "objects": scene_data["objects"],
            "relations": scene_data["relations"],
            "animation_couples": scene_data.get("animation_couples", [])
        }

        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(core_data, f, indent=4, ensure_ascii=False)

        logger.info(f"Scene saved to: {filepath}")

    def process_scene(self, text: str, language: str = "en") -> Dict[str, Any]:
        """Main processing method."""

        start_time = datetime.now()

        try:
            # Validate input
            if not text or not text.strip():
                raise Exception("Empty text provided")

            # Call OpenAI API
            api_result = self.call_openai_api(text, language)

            # Parse and validate response
            scene_data = self.parse_and_validate_response(api_result["response"])

            # Save scene data to temp/scene_output.json
            self.save_scene_to_file(scene_data)

            # Return only success status
            result = {
                "success": True
            }

            logger.info(f"Scene processed: {len(scene_data['objects'])} objects, "
                       f"{len(scene_data['relations'])} relations, "
                       f"{len(scene_data.get('animation_couples', []))} animations, "
                       f"${api_result['stats']['cost_usd']:.4f}")

            return result

        except Exception as e:
            logger.error(f"Processing failed: {str(e)}")
            return {
                "success": False,
                "error": str(e)
            }

    def get_stats_summary(self) -> Dict[str, Any]:
        """Get processing statistics summary."""

        if not self.stats:
            return {"message": "No processing statistics available"}

        total_requests = len(self.stats)
        total_cost = sum(stat.cost for stat in self.stats)
        avg_processing_time = sum(stat.processing_time for stat in self.stats) / total_requests
        total_input_tokens = sum(stat.input_tokens for stat in self.stats)
        total_output_tokens = sum(stat.output_tokens for stat in self.stats)

        models_used = {}
        for stat in self.stats:
            models_used[stat.model_used] = models_used.get(stat.model_used, 0) + 1

        return {
            "total_requests": total_requests,
            "total_cost_usd": round(total_cost, 4),
            "average_cost_per_request": round(total_cost / total_requests, 6),
            "average_processing_time": round(avg_processing_time, 2),
            "total_tokens": {
                "input": total_input_tokens,
                "output": total_output_tokens,
                "total": total_input_tokens + total_output_tokens
            },
            "models_used": models_used
        }

# Initialize processor
processor = FixedSceneProcessor()

@app.route('/process', methods=['POST'])
def process_scene_endpoint():
    """Main endpoint for processing scene descriptions."""

    try:
        # Debug request details
        logger.info(f"Request method: {request.method}")
        logger.info(f"Content-Type: {request.content_type}")
        logger.info(f"Request data: {request.data}")

        # Check if request has JSON content-type
        if not request.is_json:
            logger.warning("400 - Request is not JSON")
            return jsonify({"error": "Request must be JSON. Please set Content-Type: application/json"}), 400

        data = request.get_json(force=True)

        # Validate request
        if not data:
            logger.warning("400 - No JSON data")
            return jsonify({"error": "No JSON data provided"}), 400

        text = data.get("text", "").strip()
        language = data.get("lang", "en").lower()

        if not text:
            logger.warning("400 - No text provided")
            return jsonify({"error": "No text provided"}), 400

        logger.info(f"POST /process - {language.upper()} - {len(text)} chars")

        # Process scene
        result = processor.process_scene(text, language)

        if result["success"]:
            logger.info("200 - Success")
            return jsonify(result), 200
        else:
            logger.warning("500 - Processing failed")
            return jsonify(result), 500

    except Exception as e:
        logger.error(f"500 - Endpoint error: {str(e)}")
        return jsonify({"error": f"Processing failed: {str(e)}"}), 500

@app.route('/test-simple', methods=['POST'])
def test_simple():
    """Test with simple scene first."""
    logger.info("POST /test-simple")

    simple_test = {
        "text": "A red cube is on a blue table. Next to the table is a green chair.",
        "lang": "en"
    }

    result = processor.process_scene(simple_test["text"], simple_test["lang"])

    if result["success"]:
        logger.info("200 - Simple test success")
        return jsonify(result), 200
    else:
        logger.warning("500 - Simple test failed")
        return jsonify(result), 500

@app.route('/test-medium', methods=['POST'])
def test_medium():
    """Test with medium complexity."""
    logger.info("POST /test-medium")

    medium_test = {
        "text": "Three chairs are arranged in a row: a red chair, a blue chair, and a green chair. On a white table between them, a lamp is glowing and a cube is rotating.",
        "lang": "en"
    }

    result = processor.process_scene(medium_test["text"], medium_test["lang"])

    if result["success"]:
        logger.info("200 - Medium test success")
        return jsonify(result), 200
    else:
        logger.warning("500 - Medium test failed")
        return jsonify(result), 500

@app.route('/test-complex', methods=['POST'])
def test_complex():
    """Test with your original complex scene."""
    logger.info("POST /test-complex")

    complex_test = {
        "text": "In the center of the room, there are three wooden chairs arranged in a triangle. The first chair is large and red, the second chair is medium-sized and blue, and the third chair is small and green. Between the chairs, two glass tables are positioned: one small white table and one large black table. On the white table, there are multiple items: a tall metal lamp that is glowing softly, a glass vase containing fresh pink flowers, and two books with various colored covers. Under the black table, a small orange cat is resting on a blue cushion while a yellow ball bounces nearby. Above the entire scene, three spheres are performing a complex dance: the first sphere is red and rotates around the second sphere which is blue and floating, while the third sphere is golden and pulses with light as it orbits around the floating blue sphere.",
        "lang": "en"
    }

    result = processor.process_scene(complex_test["text"], complex_test["lang"])

    if result["success"]:
        logger.info("200 - Complex test success")
        return jsonify(result), 200
    else:
        logger.warning("500 - Complex test failed")
        return jsonify(result), 500

@app.route('/stats', methods=['GET'])
def get_processing_stats():
    """Get processing statistics."""
    logger.info("GET /stats")
    logger.info("200 - Stats retrieved")
    return jsonify(processor.get_stats_summary())

@app.route('/health', methods=['GET'])
def health_check():
    """Health check endpoint."""
    logger.info("GET /health")
    logger.info("200 - Healthy")
    return jsonify({
        "status": "healthy",
        "service": "Fixed OpenAI Scene Processor",
        "timestamp": datetime.now().isoformat(),
        "default_model": processor.default_model
    })

if __name__ == "__main__":
    print("Starting FIXED OpenAI Scene Processor...")
    print("Available endpoints:")
    print("  - POST /process")
    print("  - POST /test-simple")
    print("  - POST /test-medium")
    print("  - POST /test-complex")
    print("  - GET /stats")
    print("  - GET /health")
    print(f"Model: {processor.default_model}")
    print("="*50)

    app.run(host="0.0.0.0", port=5000, debug=False)