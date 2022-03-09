import serial

def echo_lines_until_timeout(ser: serial.Serial):
  line = ser.readline()
  while line:
    print(f'Reponse from UART:{line}')
    line = ser.readline()

# Parameters
IMAGE_PATH = '../lab1/build/kernel8.img'
COM_NAME = 'COM36'

# Open serial
ser = serial.Serial()
ser.port = COM_NAME
ser.baudrate = 115200
ser.timeout = 3
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
ser.write(b'\b\blkr_uart\n')
ser.write(bytes(f'{file_size}\n', 'ansi'))
ser.flush()
print(f'{IMAGE_PATH} size = {file_size}\n')
echo_lines_until_timeout(ser)

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

print("Python script exits here.")
