def execute_action(action):
    if action == "turn_on_light":
        print("Action: Turning on light")
        return "Light is ON"
    elif action == "play_music":
        print("Action: Playing music")
        return "Music started"
    elif action == "greeting":
        print("Action: Greeting")
        return "Hello! How can I help you?"
    else:
        print("Action: Unknown action")
        return "Action not recognized"
