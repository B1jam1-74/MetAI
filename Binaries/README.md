Binaries are made to be flashed on to the board by either using CubeProgrammer or by dropping the binary file on to the mass storage device that appears when the board is connected to the computer via USB. The binary files are located in the `Binaries` folder of this repository.

The model A binary is the first AI model that was trained and tested on the board. It only predicts if it's raining since it only serves as a proof of concept for the AI model running on the board.

The model B binary is the second AI model with the 13 classes, it doesn't perform really well however we implemented the LoRaWAN transmission in order to send the prediction of the model along with the values of the sensors. This project runs almost everything on the U545 and doesn't need any downlinks from the server's database.

The model C binary is the latest version of the project implementing interactions with the server (2 different uplinks along with 1 downlink), less classes (7 compared to the previous 13 of the model B).

Finally, the Simple board test binary is a simple test binary that can be used to verify that the board is working correctly. 