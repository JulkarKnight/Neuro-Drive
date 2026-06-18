import cv2
import dlib
import threading
import time
from scipy.spatial import distance as dist
from imutils import face_utils
import winsound
import requests  

class VideoStream:
    def __init__(self, url):
        self.stream = cv2.VideoCapture(url)
        self.stream.set(cv2.CAP_PROP_BUFFERSIZE, 1) # বাফার সাইজ সর্বনিম্ন করা
        (self.grabbed, self.frame) = self.stream.read()
        self.stopped = False

    def start(self):
        threading.Thread(target=self.update, args=()).start()
        return self

    def update(self):
        while True:
            if self.stopped:
                return
            (self.grabbed, self.frame) = self.stream.read()

    def read(self):
        return self.frame

    def stop(self):
        self.stopped = True
        self.stream.release()

# EAR
def eye_aspect_ratio(eye):
    A = dist.euclidean(eye[1], eye[5])
    B = dist.euclidean(eye[2], eye[4])
    C = dist.euclidean(eye[0], eye[3])
    ear = (A + B) / (2.0 * C)
    return ear


# WARNING: Change this IP to match your own Arduino/ESP board's IP
ALARM_BOARD_IP = "http://192.168.106.223/alarm" 


def trigger_remote_alarm():
    try:
        
        response = requests.get(ALARM_BOARD_IP, timeout=2.0)
        if response.status_code == 200:
            print("[SUCCESS] Signal delivered to Breadboard!")
    except requests.exceptions.Timeout:
        print("[WARNING] Signal sent, but Breadboard was slow to reply.")
    except Exception as e:
        print(f"[ERROR] Could not connect: {e}")

EYE_AR_THRESH = 0.23
EYE_AR_CONSEC_FRAMES = 45
COUNTER = 0
ALARM_ON = False

print("[INFO] Loading facial landmark predictor...")
detector = dlib.get_frontal_face_detector()
predictor = dlib.shape_predictor("shape_predictor_68_face_landmarks.dat")

(lStart, lEnd) = face_utils.FACIAL_LANDMARKS_IDXS["left_eye"]
(rStart, rEnd) = face_utils.FACIAL_LANDMARKS_IDXS["right_eye"]

# ESP32-CAM URL
# WARNING: Change this IP to match your ESP32-CAM's IP address
URL = "http://192.168.89.66:81/stream"

print("[INFO] Starting Threaded Video Stream...")
vs = VideoStream(URL).start()
time.sleep(2.0)

while True:
    frame = vs.read()
    if frame is None:
        continue

    frame = cv2.resize(frame, (400, 300)) 
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    rects = detector(gray, 0)

    for rect in rects:
        shape = predictor(gray, rect)
        shape = face_utils.shape_to_np(shape)

        leftEye = shape[lStart:lEnd]
        rightEye = shape[rStart:rEnd]
        
        leftEAR = eye_aspect_ratio(leftEye)
        rightEAR = eye_aspect_ratio(rightEye)
        ear = (leftEAR + rightEAR) / 2.0

        leftEyeHull = cv2.convexHull(leftEye)
        rightEyeHull = cv2.convexHull(rightEye)
        cv2.drawContours(frame, [leftEyeHull], -1, (0, 255, 0), 1)
        cv2.drawContours(frame, [rightEyeHull], -1, (0, 255, 0), 1)

        if ear < EYE_AR_THRESH:
            COUNTER += 1
            if COUNTER >= EYE_AR_CONSEC_FRAMES:
                if not ALARM_ON:
                    ALARM_ON = True
                    print("[ALERT] DROWSINESS DETECTED!")
                    
                    
                    winsound.Beep(2500, 500)
                    
                
                    threading.Thread(target=trigger_remote_alarm).start()

                cv2.putText(frame, "DROWSINESS ALERT!", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
        else:
            COUNTER = 0
            ALARM_ON = False

        cv2.putText(frame, "EAR: {:.2f}".format(ear), (300, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2)

    cv2.imshow("Neuro-Drive: Lag-Free Mode", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()
vs.stop()