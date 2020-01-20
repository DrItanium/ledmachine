(defglobal MAIN
           ?*byte-count* = 4194304 
           ?*bytes-consumed* = 0
           ?*delay-values* = (create$ 10))

(deffunction emit-instruction
             (?router 
               ?opcode
               ?byte1
               ?byte2
               ?byte3)
             (bind ?*bytes-consumed* 
                   (+ ?*bytes-consumed* 4))
             (put-char ?router
                       ?opcode)
             (put-char ?router
                       ?byte1)
             (put-char ?router
                       ?byte2)
             (put-char ?router
                       ?byte3))
(deffunction start-over
             (?router)
             (emit-instruction ?router 255 0 0 0))
(deffunction emit-instruction-with-imm24
             (?router ?opcode ?amount)
             (emit-instruction ?router
                               ?opcode
                               (bitwise-and 255 ?amount)
                               (bitwise-and 255 
                                            (right-shift ?amount
                                                         8))
                               (bitwise-and 255
                                            (right-shift ?amount
                                                         16))))
(deffunction output
             (?router ?pwm)
             (emit-instruction-with-imm24 ?router
                                          0
                                          (bitwise-and 255 ?pwm)))
(deffunction delay
             (?router ?amount)
             (emit-instruction-with-imm24
               ?router
               1
               ?amount))

(deffunction jump
             (?router ?location)
             (emit-instruction-with-imm24 ?router 2 ?location))

(progn$ (?delay ?*delay-values*)
        ;        (loop-for-count ?delay do
        ;                        (output stdout 
        ;                                255)
        ;                        (delay stdout
        ;                               1000)
        ;                        (output stdout
        ;                                0)
        ;                        (delay stdout
        ;                               1000))
        (loop-for-count (?i 0 255) do
                        (output stdout
                                ?i)
                        (delay stdout 
                               ?delay))
        (loop-for-count (?i 0 255) do
                        (output stdout
                                (- 255 ?i))
                        (delay stdout 
                               ?delay)))

; now pad out the rest of the file
(while (<> ?*bytes-consumed*
           ?*byte-count*) do
       (start-over stdout))

(exit)
