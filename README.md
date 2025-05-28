**Etapa 2**
![image](https://github.com/user-attachments/assets/185ddd55-4f65-43b5-a554-f6f7d07e2d96)
Se eliminó la generación de muros para dejar un sandbox limpio; se amplió el radio e intensidad de la linterna, se añadió luz ambiental para pruebas claras y se duplicó la velocidad del cubo. Con esta base estable, el siguiente paso es construir la malla 3D definitiva, añadir gravedad y colisiones verticales, y comenzar a integrar los power-ups y cambios de forma/textura que darán vida a las mecánicas principales.

**Etapa 3**
![image](https://github.com/user-attachments/assets/6eea6580-2202-48ce-9400-6c6594170d13)

**Final**

![image](https://github.com/user-attachments/assets/a72dc04e-5361-4bd4-9ec3-0679ec6bd6bf)

Informe de Entrega Final: Desarrollo del Juego
Jose David Martinez Oliveros 202116677
27 de mayo de 2025

Objetivo y Motivación
El objetivo principal de este proyecto ha sido el desarrollo de un juego basado en una arquitectura de renderizado 2D utilizando la biblioteca Diligent Engine, con el fin de explorar y extender las capacidades de un código base. La motivación radica en crear un entorno interactivo que combine mecánicas de plataformas con elementos visuales avanzados, como iluminación dinámica y campos de distancia firmados (SDF), para mejorar la experiencia del jugador. Este trabajo busca no solo implementar funcionalidades básicas como movimiento y colisiones, sino también innovar con desafíos por piso y efectos visuales, proporcionando una base sólida para futuros desarrollos en gráficos.

Código de Base
El código base proporcionado incluye los archivos principales Game.hpp y Game.cpp, que forman la estructura principal del juego. A continuación, se presenta un resumen del contenido:
•	Game.hpp: Define la clase Game, con métodos virtuales para inicialización, actualización, renderizado, y manejo de eventos de teclado y ratón. Incluye estructuras privadas para el jugador, el mapa, y constantes globales, así como punteros a fábricas de shaders y loaders.
•	Game.cpp: Implementa las funciones declaradas en Game.hpp, incluyendo la generación de mapas, creación de campos de distancia firmados (SDF), configuración del estado del pipeline, inicialización del jugador, vinculación de recursos, y manejo de la lógica de juego como movimiento, física y renderizado.
El código base ya ofrecía una infraestructura para renderizado 2D con soporte para texturas, shaders y buffers de constantes, pero requería expansión para incluir mecánicas de juego completas.

Análisis del Código 
La extensión del código base ha resultado en un desarrollo significativo, detallado a continuación:
•	Número de líneas: Game.cpp se ha expandido a unas 500 líneas debido a la adición de lógica de movimiento, física y generación de desafíos, y Game.hpp se coloca en unas 80 líneas, ajustándose solo en comentarios.
•	Clases: Se utiliza una única clase principal, Game, heredada de GLFWDemo. No se han añadido nuevas clases, pero las estructuras internas (m_Player, m_Map, Constants) se han enriquecido con más atributos y funcionalidades.
•	Extensión de la arquitectura: La arquitectura se ha ampliado para incluir un sistema de física vertical con gravedad y saltos dobles, controlado por variables como VertVel, JumpCount y JumpInputBufferTime. También se ha mejorado el manejo de colisiones con un algoritmo de pasos máximos (MaxCollisionSteps) y se ha añadido soporte para teletransportación como elemento interactivo.
•	Nuevos shaders: Se ha incorporado un shader de cómputo para generar el campo de distancia firmado (SDF) en CreateSDFMap, utilizando macros como RADIUS y DIST_SCALE. Además, el pipeline de renderizado (Draw) ahora utiliza un solo shader con ray marching para dibujar el mapa, el jugador y la linterna, optimizando el rendimiento.

Trabajo a futuro
Para mejorar el juego, se propone implementar completamente las púas ya referenciadas en el código, detectando colisiones con m_Map.SpikesPos en Update para aplicar daño o reiniciar al jugador, y renderizarlas con animación en Draw. Además, se sugieren power-ups que otorguen habilidades como rebotar más (aumentando JumpSpeed y JumpCount), flotar con menos gravedad (reduciendo GravityAccel), o volverse más pesado (ajustando PlayerRadius y PlayerVelocity), generados aleatoriamente en GenerateMap. Finalmente, se plantea extender GenerateMap para más niveles con variaciones en obstáculos y agregar capas 3D a m_Map.MapData, ajustando CreateSDFMap y Draw para simular una perspectiva pseudo-3D, enriqueciendo la experiencia y evolucionando hacia gráficos 3D con Diligent Engine.
