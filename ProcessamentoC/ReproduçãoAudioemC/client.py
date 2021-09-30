#!/usr/bin/env python3

""" client.py - Echo client for sending/receiving C-like structs via socket
References:
- Ctypes: https://docs.python.org/3/library/ctypes.html
- Sockets: https://docs.python.org/3/library/socket.html
"""

import socket
import sys
import random
from ctypes import *

import json
import firebase_admin
from firebase_admin import credentials
from firebase_admin import db

cred_obj = credentials.Certificate('serviceAccount.json')
default_app = firebase_admin.initialize_app(cred_obj, {
    'databaseURL': 'https://playlist-ffed8-default-rtdb.europe-west1.firebasedatabase.app/'
    })

ref = db.reference("/AudioBeamTree/Song/id/")

print("Waiting for song...")

while ref.get() == "empty":
	"""do nothing"""
	
songId = ref.get()

print("Song id:")
print(songId)

""" This class defines a C-like struct """
class Payload(Structure):
    _fields_ = [("id", c_uint32)]

def main():
    server_addr = ('localhost', 2300)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        s.connect(server_addr)
        print("Connected to {:s}".format(repr(server_addr)))

        for i in range(1):
            print("")
            payload_out = Payload(int(songId))
            nsent = s.send(payload_out)
            print("Sent {:d} bytes".format(nsent))

    except AttributeError as ae:
        print("Error creating the socket: {}".format(ae))
    except socket.error as se:
        print("Exception on socket: {}".format(se))
    finally:
        print("Closing socket")
        s.close()
        
ref.set("empty")


if __name__ == "__main__":
    main()
