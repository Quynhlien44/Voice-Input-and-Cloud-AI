import whisper
import sounddevice as sd
import numpy as np

model = whisper.load_model("tiny")

def record_audio(duration=3, fs=16000):
    print("Recording voice for", duration, "seconds...")
    audio = sd.rec(int(duration * fs), samplerate=fs, channels=1)
    sd.wait()  
    audio = np.squeeze(audio)  
    return audio

def transcribe(audio):
    result = model.transcribe(audio)
    print("Transcription:", result['text'])
    return result['text']

def record_and_recognize():
    audio = record_audio()
    return transcribe(audio)
