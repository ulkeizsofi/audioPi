import sys
if not hasattr(sys, 'argv'):
    sys.argv  = ['']

from ctypes import *
import pygame

from pygame.locals import *
import os
from time import sleep
import RPi.GPIO as GPIO
green = (34,177,76)
light_green = (0,255,0)
blackColor = (0,0,0)
redColor = (255, 0,0)
black = (0,0,0)
screen_length = 320
screen_height = 240
moving_head_dim = 10
slider_height = 10

class Button:
    def create_button(self, surface, color, x, y, length, height, width, text, text_color, idx):
        surface = self.draw_button(surface, color, length, height, x, y, width)
        surface = self.write_text(surface, text, text_color, length, height, x, y)
        self.rect = pygame.Rect(x,y, length, height)
        self.sliders = []
        self.idx = idx
        return surface

    def write_text(self, surface, text, text_color, length, height, x, y):
        if len(text) == 0:
            return surface
        
        font_size = int(length//len(text))
        myFont = pygame.font.SysFont("Calibri", font_size)
        myText = myFont.render(text, 1, text_color)
        surface.blit(myText, ((x+length/2) - myText.get_width()/2, (y+height/2) - myText.get_height()/2))
        return surface

    def draw_button(self, surface, color, length, height, x, y, width):           
        for i in range(1,10):
            s = pygame.Surface((length+(i*2),height+(i*2)))
            s.fill(color)
            alpha = (255/(i+2))
            if alpha <= 0:
                alpha = 1
            s.set_alpha(alpha)
            pygame.draw.rect(s, color, (x-i,y-i,length+i,height+i), width)
            surface.blit(s, (x-i,y-i))
        pygame.draw.rect(surface, color, (x,y,length,height), 0)
        pygame.draw.rect(surface, (190,190,190), (x,y,length,height), 1)  
        return surface

    def pressed(self, mouse):
        if mouse[0] > self.rect.topleft[0]:
            if mouse[1] > self.rect.topleft[1]:
                if mouse[0] < self.rect.bottomright[0]:
                    if mouse[1] < self.rect.bottomright[1]:
                        print "Some button was pressed!"
                        return True
                    else: return False
                else: return False
            else: return False
        else: return False

    def get_slider_values(self):
        slider_values = []
        for s in self.sliders:
            slider_values.append(s.get_value())
        return slider_values

    def get_params(self):
        return (self.idx, len(self.sliders),self.get_slider_values())

class Slider:
    def create_slider(self, surface, base_color, moving_color, x, y, length, height, width, text, text_color, min_value, max_value, idx):
        self.min = min_value
        self.max = max_value
        self.slider = pygame.Rect(x,y, length, height)
        surface = self.draw_slider(surface, base_color, moving_color, length, height, x, y, width)
        surface = self.write_text(surface, text, text_color, length, height, x, y)
        self.moving_head = pygame.Rect(x,y, moving_head_dim, height)
        self.slider_color = base_color
        self.moving_head_color = moving_color
        self.surface = surface
        self.idx = idx
        return surface

    def write_text(self, surface, text, text_color, length, height, x, y):
        if len(text) == 0:
            return surface
        
        font_size = int(length//len(text))
        text_y = y - height
        myFont = pygame.font.SysFont("Calibri", font_size)
        myText = myFont.render(text, 1, text_color)
        minText = myFont.render(str(self.min), 1, text_color)
        maxText = myFont.render(str(self.max), 1, text_color)
        surface.blit(myText, ((x+length/2) - myText.get_width()/2, text_y) )
        surface.blit(minText, (x, text_y))
        surface.blit(maxText, (x + self.slider.width - 5, text_y))
        return surface

    def draw_slider(self, surface, base_color, moving_color, length, height, x, y, width):
        for i in range(1,10):
            s = pygame.Surface((length+(i*2),height+(i*2)))
            s.fill(base_color)
            alpha = (255/(i+2))
            if alpha <= 0:
                alpha = 1
            s.set_alpha(alpha)
            pygame.draw.rect(s, base_color, (x-i,y-i,length+i,height+i), width)
            surface.blit(s, (x-i,y-i))
        pygame.draw.rect(surface, base_color, (x,y,length,height), 0)
        pygame.draw.rect(surface, moving_color, (x,y,moving_head_dim,height), 0)
        # pygame.draw.rect(surface, (190,190,190), (x,y,length,height), 1)  
        return surface

    def pressed(self, mouse):
        if mouse[0] > self.slider.topleft[0]:
            if mouse[1] > self.slider.topleft[1]:
                if mouse[0] < self.slider.bottomright[0]:
                    if mouse[1] < self.slider.bottomright[1]:
                        x = mouse[0]
                        y = mouse[1]
                        a = x
                        if a < self.slider.left:
                            a = self.slider.left
                        if a > self.slider.right:
                            a = self.slider.right
                        pygame.draw.rect(self.surface,self.slider_color,(self.slider.x, self.slider.y, self.slider.width, self.slider.height))
                        pygame.draw.rect(self.surface, self.moving_head_color, Rect(a, self.moving_head.y, self.moving_head.width, self.moving_head.height))
                        self.moving_head.x = a
                        # pygame.display.update()
                        pygame.display.update(pygame.Rect(self.slider.x, self.slider.y, self.slider.width, self.slider.height))
                        return True
                    else: return False
                else: return False
            else: return False
        else: return False

    def get_value(self):
        value = ((self.max - self.min) * (float(self.moving_head.x - self.slider.x) / (self.slider.width - self.moving_head.width)) + self.min)
        if value < self.min:
            return self.min
        elif value > self.max:
            return self.max
        else:
            return value

# def eventIterator():
#     while True:
#         yield pygame.event.wait()
#         while True:
#             event = pygame.event.poll()
#             if event.type == NOEVENT:
#                 break
#             else:
#                 yield event

def print_to_ui(labels, args, limits, fd):
    #Setup the GPIOs as outputs - only 4 and 17 are available
    # labels = arguments[0]
    # args = arguments[1]
    # w = os.fdopen(fd, 'w')
    try:
        GPIO.setwarnings(False)
        GPIO.setmode(GPIO.BCM)

        GPIO.setup(4, GPIO.OUT)
        GPIO.setup(17, GPIO.OUT)
         
        #Colours
        WHITE = (255,255,255)
         
        os.putenv('SDL_FBDEV', '/dev/fb1')
        os.putenv('SDL_MOUSEDRV', 'TSLIB')
        os.putenv('SDL_MOUSEDEV', '/dev/input/touchscreen')
         
        pygame.init()
        pygame.mouse.set_visible(False)
        lcd = pygame.display.set_mode((screen_length, screen_height))
        lcd.fill((0,0,0))
        pygame.display.update()
        font_big = pygame.font.Font(None, 20)
    except Exception as e:
        print e 
        GPIO.cleanup()
        raise
        exit()

    try:
        x_start = 20
        y_start = 20
        space = 20
        no_buttons = len(labels)
        button_height = (screen_height - 2 * x_start) / no_buttons - space
        button_length = 80
        buttons = []
        # sliders = {}
        x = x_start
        y = y_start
        for i in range(0, no_buttons):
            button = Button()
            button.create_button(lcd, (107,142,35), x, y, button_length, button_height, 0, labels[i], (255,255,255), i)
            if args[i] != 0:
                slider_length = (screen_length - button_length - space) / args[i] - space
            else:
                slider_length = 0
            slider_x = x_start + button_length + space
            # sliders[i] = []
            for j in range(0, args[i]):
                slider = Slider()
                slider.create_slider(lcd, (107,142,35), redColor, slider_x, y, button_length, moving_head_dim, 0, labels[i], (255,255,255), limits[i][j][0], limits[i][j][1], j) 
                slider_x = slider_x + slider_length + space
                # sliders[i].append(slider)
                button.sliders.append(slider)
            y = y + button_height + space
            buttons.append(button)
            # text_surface = font_big.render('%s'%k, True, WHITE)
            # rect = text_surface.get_rect(center=v)
            # lcd.blit(text_surface, rect)
        # lcd.fill((30,144,255))
        #Parameters:               surface,      color,       x,   y,   length, height, width,    text,      text_color
        pygame.display.flip()
        pygame.display.update()
         
        # while True:
        # Scan touchscreen events
        print "try to clear"
        pygame.event.clear()    
        print "cleared"
        while True:
            event = pygame.event.wait()
            if(event.type is MOUSEBUTTONDOWN):
                pos = pygame.mouse.get_pos()
                for b in buttons:
                    if b.pressed(pos):
                        print "button pressed"
                        params = b.get_params()
                        print params
                        # os.write("STARTING")
                        os.write(fd, str(params[0]))
                        os.write(fd, str(params[1]))
                        for i in range(0, params[1]):
                            os.write(fd, "{")
                            os.write(fd, str(params[2][i]));
                            os.write(fd, "}")
                        # print w
                    for s in b.sliders:
                        if s.pressed(pos):
                            print s.get_value()
        
                
                
        sleep(0.1)
    except Exception as inst:
        print inst
        raise
    finally:
        GPIO.cleanup() 
        os.close(fd)
        return 1

