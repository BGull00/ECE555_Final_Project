#!/usr/bin/env bash

#create a virtualenv
virtualenv ~/.venv

#source virtualenv
source ~/.venv/bin/activate

#append it to bash so every shell launches with it 
echo 'source ~/.venv/bin/activate' >> ~/.bashrc