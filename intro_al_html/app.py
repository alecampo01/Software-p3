from flask import Flask, request, jsonify
from flask_cors import CORS
import docx

app = Flask(__name__)
CORS(app)

BRAILLE_MAP = {
    'a': '⠁', 'b': '⠃', 'c': '⠉', 'd': '⠙', 'e': '⠑', 'f': '⠋', 'g': '⠛', 'h': '⠓', 
    'i': '⠊', 'j': '⠚', 'k': '⠅', 'l': '⠇', 'm': '⠍', 'n': '⠝', 'ñ': '⠌', 'o': '⠕', 
    'p': '⠏', 'q': '⠟', 'r': '⠗', 's': '⠎', 't': '⠞', 'u': '⠥', 'v': '⠧', 'w': '⠺', 
    'x': '⠭', 'y': '⠽', 'z': '⠵', ' ': ' ', 'á': '⠷', 'é': '⠮', 'í': '⠌', 'ó': '⠹', 'ú': '⠾'
}

def traducir_a_braille(texto):
    return "".join(BRAILLE_MAP.get(c.lower(), c) for c in texto)

def generar_gcode(texto_braille):
    gcode = [
        "G21 ; Unidades en milimetros",
        "G90 ; Posicionamiento absoluto",
        "G0 Z2.0 ; Levantar herramienta (Z seguro)"
    ]

    dot_dist_x = 2.5
    dot_dist_y = 2.5
    cell_dist_x = 6.0
    line_dist_y = 10.0
    z_safe = 2.0
    z_punch = -0.6   
    feed_rate = 150  

    dot_offsets = [
        (0, 0),                   
        (0, -dot_dist_y),         
        (0, -2 * dot_dist_y),     
        (dot_dist_x, 0),          
        (dot_dist_x, -dot_dist_y),
        (dot_dist_x, -2 * dot_dist_y) 
    ]

    current_x = 0.0
    current_y = 0.0

    for char in texto_braille:
        if char == ' ':
            current_x += cell_dist_x
            continue
        elif char == '\n':
            current_x = 0.0
            current_y -= line_dist_y
            continue

        if 0x2800 <= ord(char) <= 0x28FF:
            val = ord(char) - 0x2800
            
            for i in range(6):
                if (val & (1 << i)): 
                    x = current_x + dot_offsets[i][0]
                    y = current_y + dot_offsets[i][1]
                    
                    gcode.append(f"G0 X{x:.1f} Y{y:.1f} ; Mover sobre punto")
                    gcode.append(f"G1 Z{z_punch} F{feed_rate} ; Bajar punzon")
                    gcode.append(f"G0 Z{z_safe} ; Levantar punzon")
            
            current_x += cell_dist_x

    gcode.append("G0 X0 Y0 Z2.0 ; Volver al inicio")
    gcode.append("M30 ; Fin del programa")
    
    return "\n".join(gcode)

@app.route('/traducir', methods=['POST'])
def traducir():
    texto = ""
    
    if 'archivo' in request.files:
        file = request.files['archivo']
        doc = docx.Document(file)
        texto = " ".join([p.text for p in doc.paragraphs if p.text])
        
    elif request.is_json and 'texto' in request.json:
        texto = request.json['texto']
        
    braille_resultado = traducir_a_braille(texto)
    gcode_resultado = generar_gcode(braille_resultado)
    
    return jsonify({
        'status': 'ok',
        'braille': braille_resultado,
        'gcode': gcode_resultado
    })
    
if __name__ == '__main__':
    app.run(port=5000, debug=True)