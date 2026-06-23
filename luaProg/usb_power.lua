100 print("USB Host 5V Power Test")
110 while true do
120   -- 5V ON
130   usb.power(true)
140   print("Power ON: Please measure 5V pin")
150   delay(5000) -- 5sec wait
160   -- 5V OFF
170   usb.power(false)
180   print("Power OFF")
190   delay(5000)
200 end

