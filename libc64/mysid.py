import numpy as np
import sys
import pygame

import matplotlib.pyplot as plt

class Triangle:pass
class Sawtooth:pass
class Rectangle:pass
class Noise:pass

class Attack:pass
class Decay:pass
class Sustain:pass
class Release:pass


Freq= 15625

AttackRate=[ 2, 4, 16, 24, 38, 58, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000 ]
DecayRate = [ 6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000 ]
ReleaseRate =[6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000  ]

AttackRateOriginal=AttackRate
DecayRateOriginal =DecayRate
#convert rates to clocks
# sample time is 64us
for i in range(16):
    AttackRate[i] = 0xffffffff/((AttackRate[i]*1000)/64 )

for i in range(16):
    DecayRate[i] = 0xffffffff / ((DecayRate[i]*1000)/64) 


for i in range(16):
    ReleaseRate[i] = int(0xffffff / ((ReleaseRate[i]*1000)/64)  /np.log(2))

class Osc:
    
    def __init__(self):
        self.phase = np.uint32(0)
        self.env = np.uint32(0)
        #A
        self.fcw = np.uint16(7382)
        self.mode = Sawtooth

    def wave_out(self):
        self.phase = self.phase + self.fcw        
        osc_out = self.phase>>(10)
        
        if(self.mode == Sawtooth):
            #Saw tooth
            return osc_out & 0xff
        elif(self.mode == Triangle):
            #Triangle
            if(osc_out & 0x80):
                return (osc_out & 0x7f)*2
            else:
                return (0xff - osc_out & 0x7f)*2
        elif(self.mode == Noise):
            self.phase = self.phase^0x64fe+ (self.phase<<1)
            return (osc_out & 0xff)


    def envelope_out(self):
        if(self.env_state ==0):
            self.env = self.env + self.attack
            if(self.env > 0xffffffff):
                self.env = 0xffffffff
                self.env_state = 1
        elif(self.env_state ==1):
            self.env = self.env - self.decay
            if(self.env < self.sustain_level):
                self.env_state = 2
        elif(self.env_state ==2):
            if(not self.gate_level):
                self.env_state = 3
        elif(self.env_state ==3):
            d =( (self.env*self.release) >> 22)+1
            self.env = self.env - d
            if(self.env < 0):
                self.env = 0
                self.env_state = 5
        
        return (self.env >> 24)

        
    def gate(self, level):
        if(level):
            self.env_state = 0
        self.gate_level =level
        
    def calc_slopes(self, attack,decay,release,sustain_level):
        self.attack =AttackRate[attack]
        self.decay = DecayRate[decay]
        self.release =  ReleaseRate[release]
        self.sustain_level = sustain_level*0x10000000

        print self.attack,self.decay,self.release,self.sustain_level,DecayRate[release]


    def out(self):
        return (self.wave_out() * self.envelope_out()) >> 8
        

o1 = Osc()
o1.calc_slopes(5,8,0xa,0xa)
o1.gate(True)

N=15625*2
waveform = np.zeros(N,dtype=np.uint8)

print "Attack",np.array(AttackRate) 
print "Decay",np.array(DecayRate) 
print "Release",np.array(ReleaseRate) 

for i in range(N):
    if(i == 2000):
        o1.gate(False)

    waveform[i] = (o1.wave_out()*o1.envelope_out() ) >>8
    
pygame.mixer.init(15625, size=8, channels=1, buffer=4096)
s = pygame.mixer.Sound(waveform)
s.play()

plt.figure(1)
plt.plot(waveform)

plt.figure(2)
plt.plot(np.log(DecayRateOriginal))
plt.plot(np.log(AttackRateOriginal))
plt.show()
#while(pygame.mixer.get_busy()):
#    pass
