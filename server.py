from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import HTMLResponse, JSONResponse
from pathlib import Path
from typing import List
import requests
import base64
from io import BytesIO
from PIL import Image
import socket
import os

app = FastAPI()

TEMPLATES_DIR = Path(__file__).parent / "templates"

client_data: List[dict] = []

@app.get("/screenshot")
async def get_screenshot():
    try:
        # Создаем сокет
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            # Подключаемся к клиенту
            s.connect(("localhost", 8000))
            # Отправляем сообщение
            s.sendall(b"screenShot")
            s.close()
            print("Message 'screenShot' sent to client.")
    except Exception as e:
        print(f"Error sending message: {e}")
        return {"error": str(e)}
    
    return {"message": "Screenshot request sent to client"}

@app.get("/", response_class=HTMLResponse)
async def get_page():
    html_file_path = TEMPLATES_DIR / "index.html"
    html_content = html_file_path.read_text(encoding="utf-8")
    return HTMLResponse(content=html_content)

@app.post("/screen_upload")
async def screen_upload(request: Request):
    try:
        # Читаем тело запроса
        body = await request.json()
        ip= body.get('ip')
        # Извлекаем base64 данные и имя файла
        base64_data = body.get("screenShot")  # По умолчанию "screenshot.png"

        if not base64_data:
            raise HTTPException(status_code=400, detail="No 'base64_data' provided in request body")

        # Декодируем Base64 данные
        decoded_data = base64.b64decode(base64_data)

        # Указываем путь для сохранения файла
        save_path = os.path.join(os.getcwd(), ip+".PNG")

        # Сохраняем файл
        with open(save_path, "wb") as file:
            file.write(decoded_data)

        return {"message": f"File saved successfully as {192}", "path": save_path}
    # except json.JSONDecodeError:
    #     raise HTTPException(status_code=400, detail="Invalid JSON in request body")
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Failed to save file: {str(e)}")

@app.post("/")
async def add_client_data(request: Request):
    global client_data
    new_data = await request.json()

    for client in client_data:
        if client["clientMachine"] == new_data["clientMachine"]:
            client.update(new_data)
            return JSONResponse(content={"data": client_data})

    client_data.append(new_data)
    return JSONResponse(content={"data": client_data})

@app.get("/data")
async def get_client_data():
    global client_data
    # Объединяем данные в строку "domain/machine/ip/user"
    for client in client_data:
        client["combined"] = f'{client["clientDomain"]}/{client["clientMachine"]}/{client["clientIP"]}/{client["clientUser"]}'
    return JSONResponse(content={"data": client_data})


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=8080)