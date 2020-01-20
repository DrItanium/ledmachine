(defglobal MAIN
           ?*start-address* = 4194304 
           ?*byte-count* = 4194304 
           ?*bytes-consumed* = 0
           ?*delay-values* = (create$ 10)
           ?*opcodes* = (create$ WritePWM0
                                 DelayMilliseconds
                                 JumpAbsolute
                                 DelayMicroseconds
                                 WritePWM1
                                 WritePWM0Indirect
                                 WritePWM1Indirect
                                 StoreImmediate16IntoRegister
                                 OrdinalAdd
                                 OrdinalSubtract
                                 OrdinalMultiply
                                 OrdinalDivide
                                 OrdinalRemainder
                                 OrdinalShiftLeft
                                 OrdinalShiftRight
                                 OrdinalMin 
                                 OrdinalMax 
                                 ))

(defmethod emit-instruction
  ((?router SYMBOL)
   (?opcode INTEGER)
   (?byte1 INTEGER)
   (?byte2 INTEGER)
   (?byte3 INTEGER))
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
(defmethod emit-instruction
  ((?router SYMBOL)
   (?opcode SYMBOL)
   (?byte1 INTEGER)
   (?byte2 INTEGER)
   (?byte3 INTEGER))
  (emit-instruction ?router
                    (- (member$ ?opcode
                                ?*opcodes*)
                       1)
                    ?byte1
                    ?byte2
                    ?byte3))
(defmethod emit-instruction
  ((?router SYMBOL)
   (?opcode SYMBOL)
   (?imm24 INTEGER))
  (emit-instruction ?router
                    ?opcode
                    (bitwise-and ?imm24
                                 255)
                    (bitwise-and 255
                                 (right-shift ?imm24
                                              8))
                    (bitwise-and 255
                                 (right-shift ?imm24
                                              16))))
(defmethod pack-register-byte
  ((?lower INTEGER)
   (?upper INTEGER))
  (bitwise-or (bitwise-and 15 
                           ?lower)
              (left-shift (bitwise-and ?upper 
                                       15)
                          4)))
(defmethod pack-register-byte 
  ((?lower INTEGER))
  (pack-register-byte ?lower
                      0))
(defmethod unpack-imm16
  ((?imm16 INTEGER))
  (create$ (bitwise-and 255
                        ?imm16)
           (bitwise-and 255
                        (right-shift ?imm16 
                                     8))))
(defmethod pack-imm24
  ((?lowest INTEGER)
   (?lower INTEGER)
   (?upper INTEGER))
  (bitwise-or (bitwise-or (bitwise-and ?lowest
                                       255)
                          (left-shift (bitwise-and ?lower 
                                                   255)
                                      8))
              (left-shift (bitwise-and ?upper
                                       255)
                          16)))

(defmethod pack-imm24
  ((?lowest INTEGER)
   (?lower INTEGER))
  (pack-imm24 ?lowest
              ?lower
              0))

(deffunction output-pwm0
             (?router ?pwm)
             (emit-instruction ?router
                               WritePWM0
                               ?pwm))
(deffunction output-pwm1
             (?router ?pwm)
             (emit-instruction ?router
                               WritePWM1
                               ?pwm))
(deffunction delay
             (?router ?amount)
             (emit-instruction ?router
                               DelayMilliseconds
                               ?amount))

(deffunction jump
             (?router ?location)
             (emit-instruction ?router
                               JumpAbsolute
                               ?location))
(deffunction set-register
             (?router ?targetRegister ?value)
             (emit-instruction ?router
                               StoreImmediate16IntoRegister
                               (pack-register-byte ?targetRegister)
                               (expand$ (unpack-imm16 ?value))))
(deffunction output-pwm0-from-register
             (?router ?targetRegister)
             (emit-instruction ?router
                               WritePWM0Indirect
                               (pack-register-byte ?targetRegister)))

(deffunction output-pwm1-from-register
             (?router ?targetRegister)
             (emit-instruction ?router
                               WritePWM1Indirect
                               (pack-register-byte ?targetRegister)))



(deffunction start-over
             (?router)
             (jump ?router
                   ?*start-address*))

(deffunction three-register-op
             (?router ?opcode ?dest ?s0 ?s1)
             (emit-instruction ?router
                               ?opcode
                               (pack-imm24 (pack-register-byte ?dest 
                                                               ?s0)
                                           (pack-register-byte ?s1))))


(deffunction make-three-register-op
             (?name ?opcode)
             (build (format nil 
                            "(deffunction %s (?router ?dest ?s0 ?s1) (three-register-op ?router %s ?dest ?s0 ?s1))"
                            ?name
                            ?opcode)))
(make-three-register-op ordinal-add
                        OrdinalAdd)
(make-three-register-op ordinal-subtract
                        OrdinalSubtract)
(make-three-register-op ordinal-multiply
                        OrdinalMultiply)


(progn$ (?delay ?*delay-values*)
        (loop-for-count (?i 0 255) do
                        (set-register stdout
                                      0
                                      ?i)
                        (set-register stdout
                                      1
                                      (- 255 ?i))
                        (output-pwm0-from-register stdout 
                                                   0)
                        (output-pwm1-from-register stdout 
                                                   1)
                        (delay stdout 
                               ?delay))
        (loop-for-count (?i 0 255) do
                        (set-register stdout
                                      0
                                      ?i)
                        (set-register stdout
                                      1
                                      (- 255 ?i))
                        (output-pwm0-from-register stdout 
                                                   1)
                        (output-pwm1-from-register stdout 
                                                   0)
                        (delay stdout 
                               ?delay)))

; now pad out the rest of the file
(while (<> ?*bytes-consumed*
           ?*byte-count*) do
       (start-over stdout))

(exit)
