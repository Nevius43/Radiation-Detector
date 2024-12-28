Este proyecto forma parte de mi **Trabajo de Fin de Grado** 🎓 y consiste en el diseño y fabricación de un sistema de detección de radiación. El objetivo principal es crear un [[Contador Geiger]] adaptable a diferentes tipos de sensores, permitiendo una integración versátil y funcional.

### Sensores Utilizados 🔍

El sistema está diseñado para trabajar con diversos sensores de detección de radiación, entre los que se incluyen:

- [[Tubo Geiger]] 📡: El clásico sensor para la detección de partículas ionizantes.
- [[Centelleador]]✨: Utilizado para convertir radiación en luz detectable.
- [[Detector de barrera de silicio]] 💠: Sensor avanzado para la detección de partículas cargadas.

Todos estos sensores estarán conectados a un único puerto común, lo que facilitará su intercambio y uso según las necesidades del usuario.

### Arquitectura del Sistema 🛠️

El sistema se basa en los siguientes componentes principales:

- **Microcontrolador** [[ESP32-S3]] 🧠: Actúa como el cerebro del sistema, gestionando la adquisición de datos y el control de los sensores.
- [[Pantalla TFT]]🖥️: Proporciona una interfaz gráfica interactiva para el usuario.
- **Biblioteca** [[LVGL]] 🖌️: Se utiliza para crear una interfaz de usuario moderna y funcional en la pantalla.

### Objetivo del Diseño 🎯

El enfoque del proyecto se centra en desarrollar un sistema **adaptable y modular** que permita la conexión de cualquiera de los sensores mencionados, proporcionando una solución práctica y eficiente para la detección de radiación.