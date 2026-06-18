# Neuro-Drive Drowsiness Detection System

Neuro-Drive is an electronic project that uses computer vision to detect driver drowsiness and trigger a hardware alarm via a networked microcontroller. It combines an ESP32-CAM for video streaming, a Python script for facial landmark detection, and a secondary Arduino/ESP board with a buzzer and vibration motor for alerts.

## Project Structure

- `neuro-drive.py`: The main Python script that reads the video stream, calculates the Eye Aspect Ratio (EAR), and sends a signal to the alarm board if drowsiness is detected.
- `CameraWebServer_copy_20260507085802`: ESP32-CAM sketch for streaming video to the Python script.
- `sketch_may18a_gps`: Arduino sketch for the alarm board (handles the buzzer, vibration motor, and emergency SMS).

## Setup Instructions

### 1. Hardware Setup
1. Upload the `CameraWebServer` sketch to your ESP32-CAM.
2. Upload the `sketch_may18a_gps` sketch to your main Arduino/ESP32 board.
3. **Important**: Before uploading, make sure to change the `ssid` and `password` variables in both `.ino` files to match your local WiFi network.

### 2. Software Dependencies
You need Python installed. Install the required dependencies using:
```bash
pip install -r requirements.txt
```

### 3. Model File
The Python script requires the `shape_predictor_68_face_landmarks.dat` model file from dlib. 
Due to its large size (~100MB), it is not included in this repository. 
You can download it from [here](http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2), extract it, and place it in the same directory as `neuro-drive.py`.

### 4. Running the System
1. Power on your ESP32-CAM and Alarm board.
2. Open the Serial Monitor to find the IP addresses of both boards.
3. Open `neuro-drive.py` and update the `URL` and `ALARM_BOARD_IP` variables with the correct IP addresses.
4. Run the Python script:
   ```bash
   python neuro-drive.py
   ```

## Limitations
- The main limitation of the current setup is that the ESP32-CAM only provides the video stream, and a computer is still required to process it.

## Future Work
- Move processing to the edge directly on a capable microcontroller to eliminate the need for a separate computer.
- Further miniaturize the components for a standalone wearable or dashboard device.

## Demo
[![YouTube Video](https://img.shields.io/badge/YouTube-Watch%20Demo-red?style=for-the-badge&logo=youtube)](https://youtu.be/1CBim2Vd7dY?si=t47i2KtxFFydYJmp)
