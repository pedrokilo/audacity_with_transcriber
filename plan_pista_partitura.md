# Plan de desarrollo: pista de partitura nativa en un fork de Audacity

## 1. Resumen de lo decidido en el chat

- **Pista de partitura nativa** que convive con el audio en la misma ventana y línea de tiempo.
- **Pentagramas ilimitados y configurables**, sin instrumentos predefinidos. Cada uno tiene su propia clave, número de líneas y configuración.
- **Tres niveles**: pista → pentagrama → grupo visual (llave o corchete). Las secciones musicales cambian clave, armadura o compás en el tiempo.
- **Dos niveles de división independientes**: clips de partitura (se mueven, dividen y redimensionan) y barras de compás dentro de cada clip.
- **Vinculación con el audio opcional y desactivada por defecto**. La alineación temporal entre notas y audio se mantiene siempre.
- **Nada se inventa**: sin tempo, compás ni tonalidad obligatorios.
- **Cambiar la notación nunca modifica el audio**, y cambiar la clave no cambia la altura sonora.

## 2. Reglas de diseño que no se negocian

1. Separar tres capas: datos musicales, presentación gráfica y sincronización temporal.
2. Guardar la altura real de la nota aparte de su posición escrita.
3. Cada nota tiene posición temporal (siempre) y posición musical (solo si hay tempo y compás definidos).
4. Todo con deshacer y rehacer desde el inicio, no como parche al final.
5. Las notas manuales nunca se sobrescriben con sugerencias automáticas.

## 3. Fases

### Fase 0: Preparación (antes de escribir código de la función)

- Clonar el repositorio oficial, compilar sin modificaciones y confirmar que el entorno funciona.
- Decidir la versión base. Audacity 3.x usa wxWidgets y la línea 4.x usa Qt/QML, y esto cambia por completo cómo se dibujan pistas y se integran clips. Verifica cuál es la rama actual antes de comprometerte.
- Revisar cómo manejan las pistas, clips, selección, cursor, zoom y guardado de proyecto. Revisa también si existe o existió `NoteTrack`, y si sirve como referencia o está obsoleto en tu versión.
- **Entregable:** un documento corto con archivos, clases y puntos de integración reales.

### Fase 1: Pista vacía y persistente

- Una pista de partitura que aparece en el proyecto, dibuja un pentagrama vacío y respeta zoom, desplazamiento y cursor.
- Guardar y reabrir el proyecto con la pista.
- Cambiar altura, expandir, contraer y ocultar.
- **Hito 1:** la pista se dibuja, se sincroniza y sobrevive a guardar y abrir.

### Fase 2: Pentagramas dinámicos

- Agregar, eliminar, duplicar, renombrar, reordenar y ocultar pentagramas.
- Clave por pentagrama (sol, fa, do, percusión, sin clave) y número de líneas configurable.
- Grupos visuales con llave o corchete.
- **Hito 2:** crear 1 a N pentagramas con claves distintas, agruparlos y persistirlos.

### Fase 3: Edición de notas

- Modelo de eventos musicales: nota, silencio, altura, duración, alteraciones, puntillos y ligaduras.
- Insertar, seleccionar, mover, eliminar y cambiar duración.
- Líneas adicionales para notas fuera del registro.
- Deshacer y rehacer integrados.
- **Hito 3:** escribir una melodía a mano en dos pentagramas y recuperarla tras reabrir.

### Fase 4: Clips de partitura

- Clips independientes que se mueven, dividen y redimensionan sin tocar el audio.
- Cursor, selección y bucles compartidos con el audio.
- **Hito 4:** dividir y mover clips de partitura mientras el audio queda intacto y sincronizado.

### Fase 5: Compases y tiempo

- Barras de compás dentro del clip: añadir, mover y eliminar sin cortar el clip. Pueden ser regulares, irregulares o sin compás definido.
- Mapa de tempo, alineación musical-temporal y modos de anclaje (nota ligada a un pulso o a un instante fijo del audio).
- Configuración opcional de compás, armadura y tempo, con alcance claro: clip, selección o toda la partitura.
- **Hito 5:** definir un tempo después de haber escrito notas y ver que se reajustan sin alterar el audio.

### Fase 6: Vinculación opcional audio y partitura

- Interruptor explícito y reversible, con elección de qué elementos se vinculan.
- Comportamiento definido para dividir, mover límites y eliminar, con advertencias.
- **Hito 6:** con la vinculación apagada nada cambia, y con ella encendida las operaciones se replican de forma predecible.

### Fase 7: Estabilización

- Pruebas con muchas notas y pentagramas, y pruebas automatizadas.
- Documentar el formato de datos del proyecto.
- Revisar rendimiento del dibujo.

### Fase 8 (posterior)

- Importación y exportación MIDI, MusicXML y PDF.
- Reproducción con instrumentos virtuales.
- Asistencia automática de transcripción, detección de tempo y tonalidad.

## 4. Riesgos principales

| Riesgo | Mitigación |
|---|---|
| Elegir mal la versión base (wx vs Qt) | Decidirlo en la Fase 0 |
| Dibujo lento con muchos pentagramas o notas | Renderizar solo lo visible y medir pronto |
| Reglas de vinculación demasiado complejas | Implementarla al final, con comportamientos probados uno por uno |
| Formato de guardado que luego haya que romper | Versionar el formato desde la Fase 1 |
| Alcance que crece sin parar | Mantener la lista de "después" cerrada hasta cumplir el Hito 3 |

## 5. Próximos pasos concretos

1. Clonar y compilar Audacity.
2. Decidir la versión base.
3. Localizar en el código: la clase base de pistas, el dibujo de pistas, el manejo de clips y la serialización del proyecto.
4. Escribir el informe de arquitectura.
5. Prototipar solo un rectángulo con cinco líneas que siga el zoom y el scroll.

La primera meta es ese pentagrama vacío que se sincroniza y se guarda. Todo lo demás se apoya en eso.
