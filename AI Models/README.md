<a id="ai-models"></a>
## **AI Models**  
Both models were trained in Python (TensorFlow/Keras) on historical meteorological data sourced via [Meteostat](https://meteostat.net/ "https://meteostat.net/"), using a weather station near Le Bourget du Lac, France. They take three scalar inputs:  
<table align="center">
    <tr>
        <th>Input</th>
        <th>Unit</th>
    </tr>
    <tr><td>Temperature</td><td>&deg;C</td></tr>
    <tr><td>Relative humidity</td><td>%</td></tr>
    <tr><td>Barometric pressure</td><td>hPa</td></tr>
</table>

<p align="center">
    <img src="../Images/meteostat.png" alt="Model overview" />
</p>

### **Model A — Binary Rain Classifier**  
The first and simpler model answers a single question: **will it rain?** It outputs a sigmoid probability and is thresholded at 0.5 to produce a binary label. This model is extremely lightweight and was used as a baseline to validate the implementation of an AI model on the board. 

### **Model B — Multi-class Weather Classifier**  
The second model extends the output to **12 weather classes**, enabling a richer description of conditions. After training and quantization, it is converted to a TFLite FlatBuffer and deployed on the U545 using  **STM32Cube.AI** with the Neural-ART runtime.  
The 12 predicted classes are:  

<table align="center">
    <tr>
        <th>#</th>
        <th>French label</th>
        <th>Description</th>
        <th>International</th>
    </tr>
    <tr><td>0</td><td>Clair / ensoleillé</td><td>Clear sky</td><td>☀️</td></tr>
    <tr><td>1</td><td>Peu nuageux</td><td>Mostly sunny</td><td>🌤️</td></tr>
    <tr><td>2</td><td>Partiellement nuageux</td><td>Partly cloudy</td><td>⛅</td></tr>
    <tr><td>3</td><td>Nuageux / couvert</td><td>Overcast</td><td>☁️</td></tr>
    <tr><td>4</td><td>Pluie</td><td>Rain</td><td>🌧️</td></tr>
    <tr><td>5</td><td>Averses</td><td>Showers</td><td>🌦️</td></tr>
    <tr><td>6</td><td>Neige</td><td>Snow</td><td>❄️</td></tr>
    <tr><td>7</td><td>Neige légère / averses de neige</td><td>Light snow / snow showers</td><td>🌨️</td></tr>
    <tr><td>8</td><td>Pluie et neige mêlées</td><td>Sleet</td><td>🌨️🌧️</td></tr>
    <tr><td>9</td><td>Orage</td><td>Thunderstorm</td><td>⛈️</td></tr>
    <tr><td>10</td><td>Brouillard / brume</td><td>Fog / mist</td><td>🌫️</td></tr>
    <tr><td>11</td><td>Vent fort</td><td>Strong wind</td><td>💨</td></tr>
    <tr><td>12</td><td>Orage violent</td><td>Severe thunderstorm</td><td>🌩️</td></tr>
</table>

The firmware reads the argmax of the softmax output and encodes both the class index (predicted_class) and its French label (prediction_fr) into the LoRaWAN uplink payload.  

This model does not deliver the highest possible accuracy: predicting upcoming weather conditions from only three instantaneous meteorological measurements, without temporal context, is inherently challenging.
A key improvement would be to include additional temporal features (for example, measurements from the previous hour) to increase predictive reliability. Due to project time constraints, we could not fully validate this approach and therefore kept the current model:

<p align="center">
    <img src="../Images/model.png" alt="Model overview" />
</p>

The models were trained using the Jupyter notebooks, which are available in the `Jupyter Notebooks` folder. The training process includes data preprocessing, model architecture definition, training, and evaluation. The final TFLite model is saved as `model.tflite` and can be found in the `AI Models` folder.