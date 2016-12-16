from time import sleep

fp = open('test.log', 'w')

for x in range(1, 1000):
  fp.write(str(x) + "\n")
  fp.flush()
  sleep(2)
