# Generate lookup table for converting between perceived LED brightness and PWM

# Taken from: https://gist.github.com/mathiasvr/19ce1d7b6caeab230934080ae1f1380e
# Adapted from: https://jared.geek.nz/2013/feb/linear-led-pwm
# See also: https://ledshield.wordpress.com/2012/11/13/led-brightness-to-your-eye-gamma-correction-no/

from sys import stdout

TABLE_SIZE = 64 # 6 bit input
RESOLUTION = 256 # 8 bit output

def cie1931(L):
    L *= 100
    if L <= 8:
        return L / 902.3
    else:
        return ((L + 16) / 116)**3

x = range(0, TABLE_SIZE)
y = [cie1931(float(L) / (TABLE_SIZE - 1)) * (RESOLUTION - 1) for L in x]

stdout.write('const uint16_t CIE[%d] = {' % TABLE_SIZE)

for i, L in enumerate(y):
    if i % 16 == 0:
        stdout.write('\n')
    stdout.write('% 5d,' % round(L))

stdout.write('\n};\n')