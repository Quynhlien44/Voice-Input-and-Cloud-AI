import re

intents = {
    "turn_on_light": {
        "patterns": [
            r"turn on the light",
            r"light on"
        ],
        "action": "turn_on_light"
    },
    "play_music": {
        "patterns": [
            r"play some music",
            r"start music"
        ],
        "action": "play_music"
    },
    "greeting": {
        "patterns": [
            r"hello",
            r"hi",
        ],
        "action": "greeting"
    }
}

def detect_intent(text):
    for intent_name, data in intents.items():
        for pattern in data["patterns"]:
            if re.search(pattern, text, re.IGNORECASE):
                return intent_name, data["action"]
    return None, None
