const btnTraducir = document.getElementById('btnTraducir');
const btnCnc = document.getElementById('btnCnc');
const textoResultado = document.getElementById('textoResultado');

let ultimoGcode = "";

btnTraducir.addEventListener('click', () => {
    const inputTexto = document.querySelector('textarea').value;
    const inputArchivo = document.getElementById('archivoWord').files[0];
    const urlBackend = 'https://traductor-braille-backend.onrender.com/traducir';

    textoResultado.value = "Traduciendo y procesando G-Code...";
    const formData = new FormData();

    if (inputArchivo) {
        formData.append('archivo', inputArchivo);
        hacerPeticion(urlBackend, { method: 'POST', body: formData });
    } else if (inputTexto.trim() !== "") {
        hacerPeticion(urlBackend, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ texto: inputTexto })
        });
    }
});

async function hacerPeticion(url, opciones) {
    try {
        const respuesta = await fetch(url, opciones);
        const datos = await respuesta.json();
        
        textoResultado.value = datos.braille; // 
        ultimoGcode = datos.gcode; // 
        
    } catch (error) {
        textoResultado.value = "Error al conectar con el servidor.";
    }
}

btnCnc.addEventListener('click', () => {
    if (ultimoGcode === "") {
        alert("¡Primero debes traducir un texto para poder generar el código CNC!");
        return;
    }

    const blob = new Blob([ultimoGcode], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'trayectoria_braille.gcode';
    a.click();
    URL.revokeObjectURL(url);
});