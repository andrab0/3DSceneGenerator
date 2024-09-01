import spacy
import os
import json
import sys

# Încărcăm modelul de limbaj englez cu spaCy
nlp = spacy.load("en_core_web_sm")

# Proprietăți posibile pentru obiecte
colors = {"red", "green", "blue", "yellow", "cyan", "magenta", "white", "black", "gray", "brown"}
textures = {"matte", "shiny", "metallic"}
sizes = {"small", "large", "tiny", "huge"}

def check_collision(new_position, existing_objects, size):
    """ Verifică dacă noua poziție intră în coliziune cu obiectele existente. """
    padding = 2.0  # Spațiul suplimentar între obiecte pentru a evita coliziuni
    if size == "small":
        padding = 1.0
    elif size == "large":
        padding = 3.0
    elif size == "tiny":
        padding = 0.5
    elif size == "huge":
        padding = 4.0

    for obj in existing_objects:
        pos = obj['position']
        # Verificare simplă pe toate cele trei axe
        if abs(new_position['x'] - pos['x']) < padding and \
           abs(new_position['y'] - pos['y']) < padding and \
           abs(new_position['z'] - pos['z']) < padding:
            return True
    return False

def process_text_to_json(text, output_dir):
    doc = nlp(text)
    objects = []
    current_object = None
    current_attributes = {
        'color': None,
        'texture': None,
        'size': None
    }
    position_x, position_y, position_z = 0.0, 0.0, 0.0

    for token in doc:
        token_text = token.text.lower()

        # Identificăm atributele și le asociem cu un obiect
        if token_text in colors:
            current_attributes['color'] = token_text
        elif token_text in textures:
            current_attributes['texture'] = token_text
        elif token_text in sizes:
            current_attributes['size'] = token_text
        elif token.pos_ == "NOUN" and token_text not in {"texture", "color", "size"}:  # Evităm crearea de obiecte pentru termeni de atribut
            # Creăm un obiect nou cu atributele curente
            new_position = {'x': position_x, 'y': position_y, 'z': position_z}

            # Verificăm coliziunile și ajustăm poziția dacă e necesar
            while check_collision(new_position, objects, current_attributes['size']):
                new_position['x'] += 2.5  # Ajustăm poziția pe axa X sau poți adăuga și pe Y, Z
                # Poți adăuga logică pentru Y și Z pentru a varia pozițiile

            current_object = {
                'object': token_text,
                'color': current_attributes['color'],
                'texture': current_attributes['texture'],
                'size': current_attributes['size'],
                'position': new_position
            }
            objects.append(current_object)

            # Actualizăm pozițiile pentru următorul obiect
            position_x = new_position['x'] + 2.5

            # Resetăm atributele după ce obiectul a fost creat
            current_attributes = {
                'color': None,
                'texture': None,
                'size': None
            }

    # Structura de date pentru scenă
    scene_data = {'objects': objects}

    # Creăm directorul de output dacă nu există
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Salvăm fișierul JSON
    output_file = os.path.join(output_dir, 'scene.json')
    with open(output_file, 'w') as f:
        json.dump(scene_data, f, indent=4)

    return output_file

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: python processInput.py <text>')
        sys.exit()

    text = sys.argv[1]

    output_dir = 'temp'
    json_file = process_text_to_json(text, output_dir)
    print(f'Output file: {json_file}')
