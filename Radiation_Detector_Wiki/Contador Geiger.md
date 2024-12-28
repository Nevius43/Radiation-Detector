## **¿Qué es un Contador Geiger?** ⚛️

El **Contador Geiger-Müller**, comúnmente conocido como **contador Geiger**, es un instrumento utilizado para medir radiación ionizante. Es usado en campos como la física, la medicina y la seguridad radiológica, gracias a su capacidad para detectar partículas alfa, beta y rayos gamma. Generalmente se llama Contador Geiger al sistema que utiliza un [[Tubo Geiger]] para detectar las partículas, sin embargo en este proyecto al usar tres sensores distintos, me referiré como contador Geiger al sistema en general sin tener el cuenta el tipo de sensor.

---

## **Principio de Funcionamiento** 🧪

El funcionamiento de un contador Geiger se basa en los siguientes pasos:

1. **Detección de Radiación**:  
   La radiación ionizante interactúa con el sensor, en el caso del [[Tubo Geiger]] se produce la ionización del gas del tubo, en el caso del [[Centelleador]] se produce luz visible y en el caso del [[Detector de barrera de silicio]] se produce una pequeña corriente eléctrica.

2. **Generación de Señal**:  
   Se utiliza un circuito que convierte en fenómeno físico generado por la interacción entre el sensor y la radiación en una señal eléctrica medible mediante el ADC del [[ESP32-S3]].

3. **Interpretación de Datos**:  
   El microcontrolador traduce la señal en información útil, como el número de eventos de radiación detectados por unidad de tiempo. Esto se muestra al usuario como:
   - Clics audibles.
   - Datos visuales en una pantalla.
   - Información digital para análisis avanzado.

---

## **Componentes Clave** 🔧

Un contador Geiger generalmente consta de los siguientes elementos:

- **Sensor**: El sensor principal que detecta la radiación.
- [[Fuente de Alimentación]]: Proporciona la tensión necesaria para el funcionamiento del sensor.
- **Circuito de Amplificación y Procesamiento**: Convierte la señal del tubo en datos utilizables.
- **Interfaz de Usuario**: Puede ser una [[Pantalla TFT]], un altavoz o una conexión digital para visualizar o transmitir datos.

---

## **Aplicaciones del Contador Geiger** 💡

Los contadores Geiger tienen diversas aplicaciones, entre las que se incluyen:

- **Seguridad Radiológica**:  
  Medición de la radiación ambiental en laboratorios, hospitales o áreas contaminadas.

- **Medicina Nuclear**:  
  Control de dosis en tratamientos médicos con radioisótopos.

- **Exploración Geológica**:  
  Identificación de depósitos de minerales radiactivos.

- **Educación**:  
  Demostración de principios de radiación y detección en laboratorios educativos.
  

---
