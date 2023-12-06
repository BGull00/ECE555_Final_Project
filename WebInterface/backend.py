from typing import Union
from fastapi import FastAPI
from pydantic import BaseModel
import pandas as pd
from datetime import datetime, timedelta
from fastapi.middleware.cors import CORSMiddleware
from twilio.rest import Client

db_path = "db.csv"

def addrecord(temperature: float, humidity:float):
    timestamp = datetime.now().strftime('%m/%d/%Y %H:%M:%S')
    print(timestamp)
    df = pd.read_csv(db_path)
    df.loc[len(df.index)] = [timestamp, temperature, humidity] 
    df.to_csv(db_path,index=False)


last_temp_alert=datetime.now() - timedelta(days = 1)
last_moisture_alert=datetime.now() - timedelta(days = 1)


app = FastAPI()

origins = [
    "*",
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/")
def read_root():
    return {"Hello": "World"}


@app.post("/device/alert")
def alert(message: str):
    global last_moisture_alert
    global last_temp_alert
    account_sid = 'ACd67a106d9061d62b5196cee8ffc12ce1'
    auth_token = '07c3d984627b4e12e82801be8f526219'
    client = Client(account_sid, auth_token)
    send_alert = False
    if "Moisture" in message and (datetime.now()-last_moisture_alert).total_seconds()>60*10:
        last_moisture_alert=datetime.now()
        send_alert= True

    if "Temperature" in message and (datetime.now()-last_temp_alert).total_seconds()>60*10:
        last_temp_alert=datetime.now()
        send_alert= True

    if send_alert:
        message = client.messages.create(
        from_='+18669537040',
        body=f'Smart Farm - {message}',
        to='+18657714443')


    return {"Status":"Alert has been sent!"}

@app.post("/device/log")
def log_info(temperature: float, humidity:float):
    addrecord(temperature, humidity)
    return {"Status":"Ok"}

@app.get("/device/log")
def log_info():
    df = pd.read_csv(db_path)
    if len(df)>30:
        df = df.tail(30)
    result = [row for index, row in df.iterrows()]
    return result