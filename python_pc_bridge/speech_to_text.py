import whisper
import sounddevice as sd
import numpy as np

model = whisper.load_model("tiny")

def record_audio(duration=3, fs=16000):
    print("Recording voice for", duration, "seconds...")
    audio = sd.rec(int(duration * fs), samplerate=fs, channels=1, dtype='float32')
    sd.wait()
    audio = np.squeeze(audio)
    return audio

def transcribe(audio):
    # Whisper model expects float32 input
    if audio.dtype != np.float32:
        audio = audio.astype(np.float32)

    result = model.transcribe(audio, language='en') 
    print("Transcription:", result['text'])
    return result['text']

def record_and_recognize():
    audio = record_audio()
    return transcribe(audio)
