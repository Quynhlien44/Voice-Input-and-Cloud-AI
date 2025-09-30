import os
import openai

openai.api_key = os.getenv("OPENAI_API_KEY")

def ai_backend_response(prompt, backend="chatgpt"):
    if not prompt.strip():
        return "Empty prompt."

    try:
        response = openai.ChatCompletion.create(
            model="gpt-3.5-turbo",
            messages=[{"role": "user", "content": prompt}],
            max_tokens=150,
            temperature=0.7,
            timeout=10
        )
        return response['choices'][0]['message']['content'].strip()
    except Exception as e:
        print(f"ChatGPT API error: {e}")
        return "AI service error (ChatGPT)."
