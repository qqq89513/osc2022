import serial
import time
import sys

def echo_lines_until_timeout(ser: serial.Serial, search_for: str=None):
  line = ser.readline()
  while line:
    print(f'Reponse from UART:{line}')
    line = ser.readline()
    if search_for != None:
      index = str(line).find(search_for)
      if index != -1:
        return index
  return -1

def uart_file_size_handshake(ser: serial.Serial):
  ser.write(b'\b\blkr_uart\n')
  ser.write(bytes(f'{file_size}\n', 'ansi'))
  ser.flush()
  print(f'{IMAGE_PATH} size = {file_size}\n')
  return echo_lines_until_timeout(ser, f"Receiving {file_size} bytes...")

# Get Parameters
arg_cnt = len(sys.argv)
if arg_cnt == 1:
  print("Please specify file to send.")
  exit()
elif arg_cnt == 2:
  IMAGE_PATH = sys.argv[1]
  COM_NAME = 'COM36'
else: # arg_cnt >=3
  IMAGE_PATH = sys.argv[1]
  COM_NAME = sys.argv[2]

# Open serial
ser = serial.Serial()
ser.port = COM_NAME
ser.baudrate = 115200
ser.timeout = 1
ser.open()

# Clear buffer
ser.flush()
ser.flushInput()
ser.flushOutput()

# Get file length
arr_to_send = b''
with open(IMAGE_PATH, 'rb') as img_file:
  temp = img_file.read(1)
  while temp: # Break for b''
    arr_to_send += temp
    temp = img_file.read(1)
file_size = len(arr_to_send)

# Send file length
ret = uart_file_size_handshake(ser)
if ret == -1:
  print("Failed to handshake with rpi uart bootloader. Rebooting to retry...")
  ser.write(b'\b\breboot\n')
  time.sleep(6)
  ret = uart_file_size_handshake(ser)
  if ret == -1:
    print("Failed to handshake with rpi uart bootloader.")
    print("Python script exits here")
    exit()

# Send file
print(f'File transmitting...')
ser.write(arr_to_send)
ser.flush()
print(f'File transmitted.')
echo_lines_until_timeout(ser)

# Output last 10 byte of file
arr_int = [x for x in arr_to_send]
print(f'First 20 bytes of {IMAGE_PATH} (HEX)= ', end='')
for b in arr_int[:20]:
  print("{:02X} ".format(b), end='')
print('')

# Receive and print remaining character
echo_lines_until_timeout(ser)

print("File successfully transmitted.")
print("Python script exits here.")
