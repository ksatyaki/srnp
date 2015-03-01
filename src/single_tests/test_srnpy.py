import sys
import srnpy
import time

if __name__ == '__main__':
    srnpy.print_setup('debug')
    srnpy.initialize('SRNP_MASTER_IP=127.0.0.1', 'SRNP_MASTER_PORT=33133')
 
    srnpy.subscribe('Googler')

    time.sleep(5)
    srnpy.set_pair('simple.phooler', 'VALUE_FROM_PITHONS!')
    srnpy.print_pair_space()

    time.sleep(2)
    srnpy.shutdown()