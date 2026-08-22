-- startupA.lua
print("=== uecsOS Application Started ===")
-- Hardware initialize
local LED_PIN=13
digitalWrite(LED_PIN,0)
lcd.setCursor(0,1)
lcd.print("App Running         ")
uecs.publish("S","InCPUTemp",1,1,1,15,0.0,1,1,60)
uecs.publish("R","InAirTemp",1,1,1,15,0.0,1,1,10)
for i=1,100 do
  local Temp_val,is_valid = uecs.get("InAirTemp",1,1)
  if is_valid then
    lcd.setCursor(0,1)
    lcd.print("InAirTemp:"..tostring(Temp_val).."C   ")
    if Temp_val > 25.0 then
      digitalWrite(LED_PIN,1)
    else
      digitalWrite(LED_PIN,0)
    end
  else
    lcd.setCursor(0,1)
    lcd.print("InAirTemp: WAITING....    ")
    digitalWrite(LED_PIN,0)
  end
  delay(1000)
end
lcd.setCursor(0,1)
lcd.print("App End.............")
print("=== uecsOS Application Ended ===")
