import numpy as np
import sys
import pygame

import matplotlib.pyplot as plt
from scipy.signal import butter, lfilter
class Triangle:pass
class Sawtooth:pass
class Rectangle:pass
class Noise:pass

class Attack:pass
class Decay:pass
class Sustain:pass
class Release:pass
def butter_lowpass(cutOff, fs, order=5):
    nyq = 0.5 * fs
    normalCutoff = cutOff / nyq
    b, a = butter(order, normalCutoff, btype='low')
    return b, a


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
        self.count = 0

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


    def do_count(self):
        if(self.count==0):
            return
                        
        self.count = self.count + self.count_dir

        if((self.env_state ==1) and (self.count == self.sustain_level)):
            self.env_state = 2
            self.count_dir = 0
        elif(self.env_state ==3):
            self.decimate_reload= self.decimate_reload + 0x20*(self.release)
        elif(self.env_state ==0):
            if(self.count==0xff):
                self.decimate_reload = 1<<self.decay
                self.env_state = 1
                self.count_dir = -1
            
                    

    def envelope_out(self):
        if(self.decimate<=0):
            self.do_count()
            self.decimate = self.decimate_reload
        else:
            self.decimate = self.decimate-64
            
        return self.count
        
    def gate(self, level):
        if(level):
            self.env_state = 0
            self.count_dir = 1
            self.count = 1
            self.decimate_reload = 1<<self.attack
            self.decimate = self.decimate_reload
        else:
            print "Gate off"
            self.env_state =3
            self.count_dir = -1
            self.decimate_reload = self.release*0x20
            #self.decimate = self.decimate_reload

            
        
    def calc_slopes(self, attack,decay,release,sustain_level):
        self.attack =attack
        self.decay = decay
        self.release = release
        self.sustain_level = sustain_level*0x11
        
        print self.attack,self.decay,self.release,self.sustain_level,DecayRate[release]


    def out(self):
        return (self.wave_out() * self.envelope_out()) >> 8
        

        

    

o1 = Osc()
o1.calc_slopes(1,0x1,11,0x6)
o1.gate(True)

N=int(15625*2.4)
waveform = np.zeros(N,dtype=np.uint8)

print "Attack",np.array(AttackRate) 
print "Decay",np.array(DecayRate) 
print "Release",np.array(ReleaseRate) 

for i in range(N):
    if(i == 2000):
        o1.gate(False)

    waveform[i] = (o1.wave_out()*o1.envelope_out() ) >>8


lopass =butter_lowpass(4000,15625.0,3)
        
pygame.mixer.init(15625, size=8, channels=1, buffer=4096)
s = pygame.mixer.Sound(waveform)
s.play()

plt.figure(1)
plt.plot(waveform)

plt.figure(2)
plt.plot(np.log(DecayRateOriginal))
plt.plot(np.log(AttackRateOriginal))


order=3
for i in range(8):
    f =(i+1)*4000/(8.0)
    (b,a) = butter_lowpass(f,15625.0,order)
    
    a= a/[a[0]]
    b =b/[a[0]]

    print a,b
#    print a.astype(np.int), b #b.astype(np.int)
    print "{{",

    for j in range(1,order+1):
        print int(a[j]*0x10),",",
    print "}, {",

    for j in range(order):
        print int(b[j+1]*(1<<14)),",",
    
    print "}},";


plt.show()
#while(pygame.mixer.get_busy()):
#    pass
