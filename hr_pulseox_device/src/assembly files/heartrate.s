#VALUE INITIALIZATION (NOT NEEDED IN FINAL CODE)
addi $r1, $r0, 500		# r1 = 500 = N = BUFFER_SIZE
addi $r2, $r0, 10		# r2 = 10 = W
addi $r3, $r0, 10		# r3 = 10 ms = TIME_INTERVAL (0.01 s)

#I will be using 4 arrays.
#int arr[N], rawPeaks[N], filteredPeaks[N] - each of these will need 500 spaces in RAM which is word addressed (4 bytes)
#double s[N] - This will need 1000 spaces in RAM because it is doubles


addi $r10, $r0, 500 #Counter i for loop
addi $r11, $r0, 2 

mov_avg:
    blt $r10, $r11, mov_avg_done #r10 (500) will decrease by one every loop until its value is less than 2 (r11) and will branch out of the loop

    addi $r10, $r10, -1 #decrease the counter by 1
    addi $r12, $r0, 0 # n=0
    addi $r13, $r0, 0 # sum=0






    j mov_avg



mov_avg_done: #continue as normal