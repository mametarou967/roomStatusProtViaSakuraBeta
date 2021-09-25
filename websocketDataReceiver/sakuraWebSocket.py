# WebSocketのURI
uri = "wss://api.sakura.io/ws/v1/cloud/2e11284a-9208-49db-bd27-eaa8949e223e?access_token=2e11284a-9208-49db-bd27-eaa8949e223e"

# ライブラリのインポート
from websocket import create_connection
import json
import ambient
import datetime
import time
import traceback

receiveFinalTag = 45


class DeviceInfo():
    temp = 0.0
    humi = 0.0
    pressure = 0.0
    tvoc = 0
    eco2 = 0
    
    def __init__(self,deviceNumber):
        self.deviceNumber = deviceNumber
        
class DeviceManager():
    __device1 = DeviceInfo(1)
    __device2 = DeviceInfo(2)
    __device3 = DeviceInfo(3)
    __device4 = DeviceInfo(4)
    __deviceDummy = DeviceInfo(5)
    
    def __GetDevice(self,deviceNumber):
        if deviceNumber == 1: return self.__device1
        if deviceNumber == 2: return self.__device2
        if deviceNumber == 3: return self.__device3
        if deviceNumber == 4: return self.__device4
        return self.__deviceDummy
    
    def __UpdateTemp(self,deviceNumber,value):
        device = self.__GetDevice(deviceNumber)
        device.temp = value
    
    def __UpdateHumi(self,deviceNumber,value):
        device = self.__GetDevice(deviceNumber)
        device.humi = value
        
    def __UpdatePressure(self,deviceNumber,value):
        device = self.__GetDevice(deviceNumber)
        device.pressure = value
        
    def __UpdateTvoc(self,deviceNumber,value):
        device = self.__GetDevice(deviceNumber)
        device.tvoc = value
        
    def __UpdateEco2(self,deviceNumber,value):
        device = self.__GetDevice(deviceNumber)
        device.eco2 = value
        
    def Update(self,deviceNumber,sensorNumber,value):
        if sensorNumber == 1 : self.__UpdateTemp(deviceNumber,value)
        if sensorNumber == 2 : self.__UpdateHumi(deviceNumber,value)
        if sensorNumber == 3 : self.__UpdatePressure(deviceNumber,value)
        if sensorNumber == 4 : self.__UpdateTvoc(deviceNumber,value)
        if sensorNumber == 5 : self.__UpdateEco2(deviceNumber,value)
    
    def GetValue(self,deviceNumber,sensorNumber):
        device = self.__GetDevice(deviceNumber)
        if sensorNumber == 1 : return device.temp
        if sensorNumber == 2 : return device.humi
        if sensorNumber == 3 : return device.pressure
        if sensorNumber == 4 : return device.tvoc
        if sensorNumber == 5 : return device.eco2
        return None
        
deviceManager = DeviceManager()

while True:
	# WebSocketのURIに接続
	print("%s:[CONNECT]" % datetime.datetime.now())
	receiveFinalTagFlag = False
	keepAliveCount = 0
	
	try:
		ws = create_connection(uri)
	except:
		print("%s:[Exception Occured]" % datetime.datetime.now())
		traceback.print_exc()
		print("%s:[Wait For Exception]" % datetime.datetime.now())
		time.sleep(30)
	
	while True:
		result = ''
		# WebSocketでデータを受信
		try:
			result = ws.recv()
		except:
			print("%s:[Exception Occured]" % datetime.datetime.now())
			traceback.print_exc()
			break
		
		print("%s:Received '%s'" % (datetime.datetime.now(),result))
		json_dict = json.loads(result)
		
		# receive packet transaction
		if not 'keepalive' in json_dict['type']:
			print("%s:[Valid Data Receive]" % datetime.datetime.now())
			# from device
			tag = json_dict['payload'][0]['tag']
			deviceNumber = int(tag) // 10
			sensorNumber = int(tag) % 10
			deviceManager.Update(deviceNumber,sensorNumber,json_dict['payload'][0]['value'])
			if int(tag) is receiveFinalTag:
				print("%s:[receiveFinalTag]" % datetime.datetime.now())
				receiveFinalTagFlag = True
		else:
			print("%s:[Keep Alive Receive]" % datetime.datetime.now())
			## keepalive
			keepAliveCount = keepAliveCount + 1
		
		#send Transaction
		if receiveFinalTagFlag is True:
			print("%s:[Send to ambient]" % datetime.datetime.now())
			#ch1
			ambi1 = ambient.Ambient(41295, "ca34f6009f3815d9") 
			ambi1.send({ 
				"d1": deviceManager.GetValue(1,1),
				"d2": deviceManager.GetValue(2,1),
				"d3": deviceManager.GetValue(3,1),
				"d4": deviceManager.GetValue(4,1),
				})
	

			#ch2
			ambi2 = ambient.Ambient(41721, "2f6ca439ebd734ee") 
			ambi2.send({
				"d1": deviceManager.GetValue(1,2),
				"d2": deviceManager.GetValue(2,2),
				"d3": deviceManager.GetValue(3,2),
				"d4": deviceManager.GetValue(4,2),
				})
			
			#ch3
			ambi3 = ambient.Ambient(41722, "2f45be6ae53f990d") 
			ambi3.send({
				"d1": deviceManager.GetValue(1,3),
				"d2": deviceManager.GetValue(2,3),
				"d3": deviceManager.GetValue(3,3),
				"d4": deviceManager.GetValue(4,3),
				})
				
			
			#ch4
			ambi4 = ambient.Ambient(41723, "72e6538c6206e335") 
			ambi4.send({
				"d1": deviceManager.GetValue(1,4),
				"d2": deviceManager.GetValue(2,4),
				"d3": deviceManager.GetValue(3,4),
				"d4": deviceManager.GetValue(4,4),
				})
				
			
			#ch5
			ambi5 = ambient.Ambient(41724, "1b66c605f666578d") 
			ambi5.send({
				"d1": deviceManager.GetValue(1,5),
				"d2": deviceManager.GetValue(2,5),
				"d3": deviceManager.GetValue(3,5),
				"d4": deviceManager.GetValue(4,5),
				})
		
		#terminate condition
		if receiveFinalTagFlag is True:
			print("%s:[receiveFinalTagFlag is True -> ws close]" % datetime.datetime.now())
			break
		
		if keepAliveCount > 20:
			print("%s:[keepAliveCount is over -> ws close]" % datetime.datetime.now())
			break;

	print("%s:[CLOSE]" % datetime.datetime.now())
	ws.close()
