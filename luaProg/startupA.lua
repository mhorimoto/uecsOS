-- startupA.lua
print("=== uecsOS Application Started ===")
-- Hardware initialize
local LED_PIN=13
digitalWrite(LED_PIN,0)
lcd.setCursor(0,1)
lcd.print("App Running         ")
uecs.publish("S","InCPUTemp",1,1,1,15,0.0,1,1,60)
uecs.publish("R","InAirTemp",1,1,1,15,0.0,1,1,10)
lcd.setCursor(0,1)
lcd.print("App End.............")
print("=== uecsOS Application Ended ===")
