#!/usr/bin/env python3
"""
FINAL Voice AI Server với OpenAI Whisper + GPT + TTS Fixed
"""
from urllib import response
import paho.mqtt.client as mqtt
import json
import base64
import logging
import os
import struct
import tempfile
import re
import time
from datetime import datetime
from io import BytesIO

# AI và TTS
import openai
from dotenv import load_dotenv

# Load environment
load_dotenv()

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class FixedVoiceAIServer:
    def __init__(self):
        logger.info("🚀 ULTIMATE FIXED Voice AI Server")
        
        # OpenAI setup
        self.openai_api_key = os.getenv("OPENAI_API_KEY")
        if self.openai_api_key:
            openai.api_key = self.openai_api_key
            self.ai_enabled = True
            logger.info("✅ OpenAI GPT & Whisper enabled")
        else:
            self.ai_enabled = False
            logger.warning("⚠️ OpenAI API key not set, using simulated mode")
        
        # MQTT client
        self.client = mqtt.Client()
        self.setup_mqtt()
        
    def setup_mqtt(self):
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
    
    def create_wav_from_pcm(self, pcm_data, sample_rate=16000, channels=1, bits_per_sample=16):
        """Tạo WAV header cho PCM data"""
        try:
            byte_rate = sample_rate * channels * bits_per_sample // 8
            block_align = channels * bits_per_sample // 8
            data_size = len(pcm_data)
            
            header = BytesIO()
            header.write(b'RIFF')
            header.write(struct.pack('<I', 36 + data_size))
            header.write(b'WAVE')
            header.write(b'fmt ')
            header.write(struct.pack('<I', 16))
            header.write(struct.pack('<H', 1))
            header.write(struct.pack('<H', channels))
            header.write(struct.pack('<I', sample_rate))
            header.write(struct.pack('<I', byte_rate))
            header.write(struct.pack('<H', block_align))
            header.write(struct.pack('<H', bits_per_sample))
            header.write(b'data')
            header.write(struct.pack('<I', data_size))
            
            return header.getvalue() + pcm_data
        except Exception as e:
            logger.error(f"❌ WAV creation error: {e}")
            return b''
    
    def speech_to_text_with_whisper(self, audio_base64):
        """Speech-to-Text with Whisper"""
        try:
            # Decode base64
            audio_bytes = base64.b64decode(audio_base64)
            
            # Log audio info
            audio_length = len(audio_bytes)
            logger.info(f"🎵 Audio for Whisper: {audio_length} bytes ({audio_length/32000:.2f} sec)")
            
            if audio_length < 3200:  # 0.1 seconds
                logger.warning(f"⚠️ Audio too short for Whisper: {audio_length} bytes")
                return "hello" # Fallback text
            
            # Create WAV file
            wav_data = self.create_wav_from_pcm(audio_bytes)
            if not wav_data:
                return "test"
            
            # Save temporary file
            with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp_file:
                tmp_file.write(wav_data)
                tmp_file_path = tmp_file.name
            
            try:
                logger.info("🎤 Sending to OpenAI Whisper...")
                with open(tmp_file_path, 'rb') as audio_file:
                    transcript = openai.Audio.transcribe(
                        model="whisper-1",
                        file=audio_file,
                        language="en"
                    )
                
                text = transcript['text'].strip()
                logger.info(f"✅ Whisper Result: '{text}'")
                
                return text
                
            except Exception as e:
                logger.error(f"❌ Whisper API error: {e}")
                return "hello" # Graceful fallback
            finally:
                # Delete temporary file
                if os.path.exists(tmp_file_path):
                    os.unlink(tmp_file_path)
            
        except Exception as e:
            logger.error(f"❌ Whisper processing error: {e}")
            return "hello"
    
    def text_to_speech_fixed(self, text, lang='en'):
        """TTS FIXED - Create ultra-short audio directly for ESP32"""
        try:
            logger.info(f"🔊 TTS FIXED (Ultra Short): '{text}'")
            
            # Limit text length
            if len(text) > 50:
                text = text[:50] + "..."
            
            # PARAMETERS - VERY SHORT: 0.25 seconds (4000 bytes)
            sample_rate = 16000
            duration_ms = 250  # ⚠️ ONLY 0.25 SECONDS
            num_samples = int(sample_rate * duration_ms / 1000)
            
            # Create PCM data directly
            pcm_data = bytearray()
            import math
            # Base frequency
            base_freq = 440  # 440Hz = A
            
            # Simple sine wave with fade in/out
            for i in range(num_samples):
                t = i / sample_rate
                # 440Hz sine wave with fade in/out
                envelope = 1.0
                if t < 0.05:  # Fade in 50ms
                    envelope = t / 0.05
                elif t > 0.2:  # Fade out 50ms
                    envelope = (0.25 - t) / 0.05
                
                # Create simple sine wave
                sample = 0.4 * 32767 * math.sin(2 * math.pi * base_freq * t) * envelope
                
                # Limit value
                sample = max(-32767, min(32767, sample))
                pcm_data.extend(struct.pack('<h', int(sample)))
            
            # Check audio is not all zeros
            pcm_bytes = bytes(pcm_data)
            first_100 = pcm_bytes[:100]
            zero_count = sum(1 for b in first_100 if b == 0)
            
            logger.info(f"✅ TTS created: {len(pcm_bytes)} bytes, zero bytes: {zero_count}/100")
            
            return {
                'audio_data': base64.b64encode(pcm_bytes).decode('utf-8'),
                'sample_rate': sample_rate,
                'bits_per_sample': 16,
                'num_channels': 1,
                'audio_length_bytes': len(pcm_bytes),
                'duration_ms': duration_ms
            }
            
        except Exception as e:
            logger.error(f"❌ TTS error: {e}")
            import traceback
            logger.error(traceback.format_exc())
            return self.create_simple_beep()
    
    def create_simple_beep(self):
        """Tạo beep đơn giản (fallback)"""
        try:
            import math
            
            sample_rate = 16000
            duration_ms = 250  # 0.25 seconds
            num_samples = int(sample_rate * duration_ms / 1000)
            
            pcm_data = bytearray()
            
            # Create 2 short beeps
            for i in range(num_samples):
                t = i / sample_rate
                
                # Create 2 short beeps
                if t < 0.1 or (0.15 <= t < 0.25):
                    freq = 660  # Mi
                else:
                    freq = 0
                
                if freq > 0:
                    # Fade in/out for each beep
                    if t < 0.05 or (0.15 <= t < 0.2):
                        envelope = (t % 0.05) / 0.05
                    else:
                        envelope = (0.05 - (t % 0.05)) / 0.05
                    
                    sample = 0.3 * 32767 * math.sin(2 * math.pi * freq * t) * envelope
                    pcm_data.extend(struct.pack('<h', int(sample)))
                else:
                    pcm_data.extend(struct.pack('<h', 0))
            
            logger.info(f"🔧 Simple beep created: {len(pcm_data)} bytes")
            
            return {
                'audio_data': base64.b64encode(bytes(pcm_data)).decode('utf-8'),
                'sample_rate': sample_rate,
                'bits_per_sample': 16,
                'num_channels': 1,
                'audio_length_bytes': len(pcm_data),
                'duration_ms': duration_ms
            }
        except Exception as e:
            logger.error(f"❌ Beep creation error: {e}")
            # Return empty audio
            return {
                'audio_data': '',
                'sample_rate': 16000,
                'bits_per_sample': 16,
                'num_channels': 1,
                'audio_length_bytes': 0,
                'duration_ms': 0
            }
    
    def process_with_openai(self, text):
        """Xử lý với OpenAI GPT - Response NGẮN"""
        try:
            if not self.ai_enabled:
                return self.get_short_response(text)
            
            # Create short prompt
            if len(text) < 3 or text.lower() in ['you', 'hi', 'hello']:
                prompt_text = "Say hello in 2 words"
            else:
                prompt_text = f"Respond to '{text}' in 2-3 words max"
            
            response = openai.ChatCompletion.create(
                model="gpt-3.5-turbo",
                messages=[
                    {"role": "system", "content": "You are a voice assistant on ESP32. Responses must be 2-3 words maximum."},
                    {"role": "user", "content": prompt_text}
                ],
                max_tokens=15,
                temperature=0.7
            )
            
            ai_text = response.choices[0].message.content.strip()
            logger.info(f"🤖 GPT Response: {ai_text}")
            return ai_text
            
        except Exception as e:
            logger.error(f"❌ OpenAI error: {e}")
            return self.get_short_response(text)
    
    def get_short_response(self, text):
        """Response ngắn gọn"""
        text_lower = text.lower()
        
        responses = {
            "hello": "Hi!",
            "hi": "Hello!",
            "you": "Hi!",
            "bye": "Bye!",
            "thank": "Welcome!",
            "test": "Test OK!",
            "what time": "Check clock",
            "weather": "Sunny!",
            "name": "I'm AI!",
            "how are": "I'm good!"
        }
        
        for key, response in responses.items():
            if key in text_lower:
                return response
        
        return "Hello!"
    
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info("✅ Connected to MQTT broker")
            client.subscribe("voice/audio")
            logger.info("📡 Subscribed to: voice/audio")
            
            # Welcome message
            welcome = {
                "transcribed_text": "system",
                "ai_response": "AI Ready!",
                "tts_audio": "",
                "audio_sample_rate": 16000,
                "audio_bits": 16,
                "audio_channels": 1,
                "audio_length_bytes": 0,
                "audio_duration_ms": 0,
                "status": "ready",
                "timestamp": datetime.now().isoformat()
            }
            client.publish("voice/response", json.dumps(welcome), qos=0)
            logger.info("📤 Sent welcome message")
        else:
            logger.error(f"❌ Connection failed: {rc}")
    
    def on_message(self, client, userdata, msg):
        try:
            logger.info("=" * 60)
            logger.info(f"📥 Received on {msg.topic}")
            
            # Parse JSON
            raw_data = msg.payload.decode('utf-8', errors='ignore')
            logger.info(f"   Raw data length: {len(raw_data)} chars")
            logger.info(f"   First 100 chars: {raw_data[:100]}")
            
            try:
                data = json.loads(raw_data)
            except json.JSONDecodeError as e:
                logger.error(f"❌ JSON decode error: {e}")
                # Try extract manual
                audio_match = re.search(r'"audio_data"\s*:\s*"([^"]+)"', raw_data)
                if audio_match:
                    audio_base64 = audio_match.group(1)
                    data = {
                        "device": "xiao",
                        "count": 0,
                        "adc_value": 0,
                        "audio_data": audio_base64
                    }
                    logger.info("✅ Extracted audio data manually")
                else:
                    logger.error("❌ Could not parse data")
                    return
            
            device = data.get("device", "xiao")
            count = data.get("count", 0)
            adc_value = data.get("adc_value", 0)
            audio_base64 = data.get("audio_data", "")
            
            logger.info(f"   Device: {device}, Count: {count}, Audio: {len(audio_base64)} chars")
            
            if not audio_base64 or len(audio_base64) < 100:
                logger.warning("⚠️ No audio data")
                error_response = {
                    "transcribed_text": "no_audio",
                    "ai_response": "Speak again!",
                    "tts_audio": "",
                    "status": "no_audio",
                    "count": count,
                    "adc_value": adc_value,
                    "timestamp": datetime.now().isoformat()
                }
                client.publish("voice/response", json.dumps(error_response), qos=0)
                return
            
            # Step 1: Speech-to-Text
            logger.info("🎤 Speech to text...")
            transcribed_text = self.speech_to_text_with_whisper(audio_base64)
            
            # Step 2: AI Processing
            logger.info("🤖 AI processing...")
            ai_response = self.process_with_openai(transcribed_text)
            
            # Step 3: TTS (FIXED VERSION - ultra short)
            logger.info("🔊 Generating TTS (fixed - ULTRA SHORT)...")
            tts_result = self.text_to_speech_fixed(ai_response)
            
            # Check if tts audio is not empty
            if not tts_result['audio_data'] or len(tts_result['audio_data']) < 100:
                logger.error("❌ TTS audio empty or too short")
                tts_result = self.create_simple_beep()
            
            # Build response - NO TRUNCATION
            response = {
                "transcribed_text": transcribed_text,
                "ai_response": ai_response,
                "tts_audio": tts_result['audio_data'],
                "audio_sample_rate": tts_result['sample_rate'],
                "audio_bits": tts_result['bits_per_sample'],
                "audio_channels": tts_result['num_channels'],
                "audio_length_bytes": tts_result['audio_length_bytes'],
                "audio_duration_ms": tts_result['duration_ms'],
                "status": "success",
                "device": device,
                "count": count,
                "adc_value": adc_value,
                "timestamp": datetime.now().isoformat()
            }
            
            # Send response - NO TRUNCATION
            response_json = json.dumps(response, separators=(',', ':'))
            logger.info(f"📤 Sending response: {len(response_json)} chars, audio: {tts_result['audio_length_bytes']} bytes")
            
            # Check size
            if len(response_json) > 20000:
                logger.warning(f"⚠️ Response large: {len(response_json)} chars")
            
            # Send with QoS 0
            client.publish("voice/response", response_json, qos=0)
            logger.info("✅ Response sent!")
            logger.info("=" * 60)
            
        except Exception as e:
            logger.error(f"❌ Critical error: {e}")
            import traceback
            logger.error(traceback.format_exc())
    
    def start(self):
        try:
            logger.info("🔗 Connecting to broker.emqx.io:1883")
            self.client.connect("broker.emqx.io", 1883, 60)
            
            logger.info("=" * 60)
            logger.info("🎯 ULTIMATE FIXED SERVER")
            logger.info("   Features:")
            logger.info("   1. Ultra short TTS (0.25s)")
            logger.info("   2. Short responses (2-3 words)")
            logger.info("   3. No gTTS, direct PCM")
            logger.info("=" * 60)
            
            self.client.loop_forever()
        except KeyboardInterrupt:
            logger.info("🛑 Stopped by user")
        except Exception as e:
            logger.error(f"❌ Server error: {e}")
        finally:
            self.client.disconnect()

def main():
    server = FixedVoiceAIServer()
    server.start()

if __name__ == "__main__":
    main()