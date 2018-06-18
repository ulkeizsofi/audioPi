import sys
if not hasattr(sys, 'argv'):
    sys.argv  = ['']

from ctypes import *
import pygame

from pygame.locals import *
import os
from time import sleep
import RPi.GPIO as GPIO
margin = 20
space = 20
green = (34,177,76)
some_green = (107,142,35)
light_green = (0,255,0)
blackColor = (0,0,0)
dark_green = (0, 68, 0)
red_color = (255, 0,0)
black = (0,0,0)
screen_length = 320
screen_height = 240
moving_head_dim = 10
slider_height = 10
slider_width = screen_length - 3*margin
settings_size = 20
max_letter_size = 20
settings_image = pygame.image.load("settings.png")
settings_image = pygame.transform.scale(settings_image, (settings_size, settings_size))
back_button_idx = -1
back_button_text ="BACK"
reset_idx = 5

class Button:
    def __init__(self, idx, surface, active_color, inactive_color, x, y, height, length, text, text_color, is_effect_button):
        self.surface = surface
        self.rect = pygame.Rect(x,y, length, height)
        self.active_color = active_color
        self.inactive_color = inactive_color
        self.text = text
        self.text_color = text_color
        self.idx = idx
        if is_effect_button:
            self.active = False
        else:
            self.active = True #it remains true all the time
        self.is_effect_button = is_effect_button

    def write_text(self):
        if len(self.text) == 0:
            return 
        
        font_size = int(self.rect.width//len(self.text))
        myFont = pygame.font.SysFont("Calibri", font_size)
        myText = myFont.render(self.text, 1, self.text_color)
        self.surface.blit(myText, ((self.rect.x+self.rect.width/2) - myText.get_width()/2, (self.rect.y+self.rect.height/2) - myText.get_height()/2))
        return

    def draw_button(self):           
        color = self.active_color if self.is_effect_button and self.active else self.inactive_color
        for i in range(1,10):
            s = pygame.Surface((self.rect.width+(i*2),self.rect.height+(i*2)))
            s.fill(color)
            alpha = (255/(i+2))
            if alpha <= 0:
                alpha = 1
            s.set_alpha(alpha)
            pygame.draw.rect(s, color, (self.rect.x-i,self.rect.y-i,self.rect.width+i,self.rect.height+i), self.rect.width)
            self.surface.blit(s, (self.rect.x-i,self.rect.y-i))
        pygame.draw.rect(self.surface, color, (self.rect.x,self.rect.y,self.rect.width,self.rect.height), 0)
        pygame.draw.rect(self.surface, (190,190,190), (self.rect.x,self.rect.y,self.rect.width,self.rect.height), 1)  
        self.write_text()

    def pressed(self, mouse):
        if mouse[0] > self.rect.topleft[0]:
            if mouse[1] > self.rect.topleft[1]:
                if mouse[0] < self.rect.bottomright[0]:
                    if mouse[1] < self.rect.bottomright[1]:
                        print "Some button was pressed!"
                        if self.is_effect_button:
                            self.invert_activ_status()
                        return True
                    else: return False
                else: return False
            else: return False
        else: return False

    def invert_activ_status(self):
        self.active = not self.active
        self.draw_button()

    def set_activ_status(self, stat):
        self.active = stat
        self.draw_button()        

class Slider:
    def __init__(self, surface, base_color, moving_color, x, y, height, length, text, text_color, min_value, max_value, idx):
        self.min = min_value
        self.max = max_value
        self.slider = pygame.Rect(x,y, length, height)
        self.moving_head = pygame.Rect(x,y, moving_head_dim, height)
        self.slider_color = base_color
        self.moving_head_color = moving_color
        self.surface = surface
        self.idx = idx
        self.text = text
        self.text_color = text_color
        # surface = self.draw_slider(surface, base_color, moving_color, x, y, height, length)
        # surface = self.write_text(surface, text, text_color, x, y, height, length)

    def write_text(self):
        if len(self.text) == 0:
            return        
        font_size = int(self.slider.width//len(self.text))
        font_size = font_size if font_size < max_letter_size else max_letter_size
        text_y = self.slider.y - self.slider.height
        myFont = pygame.font.SysFont("Calibri", font_size)
        myText = myFont.render(self.text, 1, self.text_color)
        minText = myFont.render(str(self.min), 1, self.text_color)
        maxText = myFont.render(str(self.max), 1, self.text_color)
        self.surface.blit(myText, ((self.slider.x+self.slider.width/2) - myText.get_width()/2, text_y) )
        self.surface.blit(minText, (self.slider.x, text_y))
        self.surface.blit(maxText, (self.slider.x + self.slider.width - 5, text_y))

    def draw_slider(self):
        for i in range(1,10):
            s = pygame.Surface((self.slider.width+(i*2),self.slider.height+(i*2)))
            s.fill(self.slider_color)
            alpha = (255/(i+2))
            if alpha <= 0:
                alpha = 1
            s.set_alpha(alpha)
            pygame.draw.rect(s, self.slider_color, (self.slider.x-i,self.slider.y-i,self.slider.width+i,self.slider.height+i), self.slider.width)
            self.surface.blit(s, (self.slider.x-i,self.slider.y-i))
        pygame.draw.rect(self.surface, self.slider_color, (self.slider.x,self.slider.y,self.slider.width,self.slider.height), 0)
        pygame.draw.rect(self.surface, self.moving_head_color, (self.moving_head.x,self.moving_head.y,moving_head_dim,self.slider.height), 0)
        self.write_text()

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
                        self.moving_head.x = a
                        self.draw_slider();
                        # pygame.draw.rect(self.surface,self.slider_color,(self.slider.x, self.slider.y, self.slider.width, self.slider.height))
                        # pygame.draw.rect(self.surface, self.moving_head_color, Rect(a, self.moving_head.y, self.moving_head.width, self.moving_head.height))
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

class Effect:
    def __init__(self, idx, surface, color1, color2, button_x, button_y, button_height, button_length, text, text_color, no_sliders, has_settings, labels, limits):
        self.idx = idx
        self.no_sliders = no_sliders
        self.has_settings = has_settings
        self.surface = surface
        
        
        #create button 
        if idx == reset_idx: #Reset button
            is_effect = False
        else:
            is_effect = True
        self.button = Button(idx, surface, color1, color2, button_x, button_y, button_height, button_length, text, text_color, is_effect)
        # self.button.draw_button()

        #create settings
        if self.has_settings:
            self.settings = self.attach_settings(surface)

        #create sliders
        self.sliders = []
        slider_x = margin
        slider_y = margin + max_letter_size
        for j in range(0, self.no_sliders):
            slider = Slider(self.surface, some_green, red_color, slider_x, slider_y, moving_head_dim, slider_width, labels[self.idx], (255,255,255), limits[self.idx][j][0], limits[self.idx][j][1], j)
            slider_y = slider_y + moving_head_dim + space
            # sliders[i].append(slider)
            self.sliders.append(slider)

    def attach_settings(self, surface):
        print self.button.idx, self.no_sliders
        if self.no_sliders == 0:
            return
        x = self.button.rect.x + self.button.rect.width + space
        y = self.button.rect.y
        return pygame.Rect(x, y, space, space)

    def draw_settings(self):
        if self.has_settings:
            self.surface.blit(settings_image, ( self.settings.x, self.settings.y) ) 

    def settings_pressed(self, mouse):
        if not self.has_settings:
            return False
        if mouse[0] > self.settings.topleft[0]:
            if mouse[1] > self.settings.topleft[1]:
                if mouse[0] < self.settings.bottomright[0]:
                    if mouse[1] < self.settings.bottomright[1]:
                        print "Settings was pressed!"
                        return True
                    else: return False
                else: return False
            else: return False
        else: return False        

    def draw_sliders(self):
        if self.no_sliders != 0:
            slider_length = screen_length
        else:
            return
        self.surface.fill((0,0,0))
        for s in self.sliders:
            s.draw_slider()

    def get_slider_values(self):
        slider_values = []
        for s in self.sliders:
            slider_values.append(s.get_value())
        return slider_values

    def get_params(self):
        return (self.idx, len(self.sliders),self.get_slider_values())

class Window:
    def __init__(self, surface, labels, args, limits, fd):
        print args
        self.no_effects = len(labels)
        self.button_height = (screen_height - 2 * margin) / self.no_effects - space
        self.button_length = 80
        self.labels = labels
        self.args = args
        self.limits = limits
        self.fd = fd
        self.surface = surface

        #create effects
        self.effects = []
        x = margin
        y = margin
        for i in range(0, self.no_effects):
            #def __init__(self, idx, surface, color, x, y, height, length, text, text_color, no_sliders):
            has_settings = False if self.args[i] == 0 else True
            effect = Effect(i, self.surface, dark_green, some_green, x, y, self.button_height, self.button_length, self.labels[i], (255,255,255), self.args[i], has_settings, labels, limits)
            y = y + self.button_height + space
            self.effects.append(effect)
        self.back_button = Button(back_button_idx, self.surface, red_color, some_green, screen_length / 2 - self.button_length / 2, screen_height - margin - self.button_height, self.button_height, self.button_length, back_button_text, blackColor, False)
        self.effects_visible = True
        self.settings_visible = None

    def draw_effect_buttons(self):
        self.surface.fill(blackColor)
        for e in self.effects:
            e.button.draw_button()
            e.draw_settings()

    def draw_effect_settings(self, e):
        self.surface.fill(blackColor)
        e.draw_sliders()
        self.back_button.draw_button()
        

    def handle_click(self, pos):
        fd = self.fd
        for e in self.effects:
            if self.effects_visible:
                if e.button.pressed(pos):
                    if e.button.active:
                        self.activate_an_effect(e)
                    else:   
                        self.dezactivate_an_effect(e)
                else:
                    if e.settings_pressed(pos):
                        self.draw_effect_settings(e)
                        pygame.display.flip()
                        pygame.display.update()
                        self.effects_visible = False
                        self.settings_visible = e.settings

            elif e.has_settings and self.settings_visible == e.settings:
                for s in e.sliders:
                    if s.pressed(pos):
                        print s.get_value()
                if self.back_button.pressed(pos):
                    self.draw_effect_buttons();
                    self.effects_visible = True
    def dezactivate_an_effect(self, e):
        #Send the reset to the processing program
        params = self.effects[reset_idx].get_params()
        print "reset", params
        write_to_pipe(self.fd, params)
        #Activate all the other effects
        for e in self.effects:
            if e.button.is_effect_button and e.button.active:
                params = e.get_params()
                write_to_pipe(self.fd, params)

    def activate_an_effect(self, e):
        params = e.get_params()
        write_to_pipe(self.fd, params)
        if e.button.idx == reset_idx:
            for effect_to_reset in self.effects:
                if effect_to_reset.button.is_effect_button:
                    effect_to_reset.button.set_activ_status(False)

def write_to_pipe(fd, params):
    os.write(fd, str(params[0]))
    os.write(fd, str(params[1]))
    for i in range(0, params[1]):
        os.write(fd, "{")
        os.write(fd, str(params[2][i]));
        os.write(fd, "}")
    
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
        window = Window(lcd, labels, args, limits, fd)
        window.draw_effect_buttons()
        pygame.display.flip()
        pygame.display.update()
         
        # while True:
        # Scan touchscreen events
        pygame.event.clear()    
        while True:
            event = pygame.event.wait()
            if(event.type is MOUSEBUTTONDOWN):
                pos = pygame.mouse.get_pos()
                window.handle_click(pos)
                pygame.display.update()
        
                
                
        sleep(0.1)
    except Exception as inst:
        print inst
        raise
    finally:
        GPIO.cleanup() 
        os.close(fd)
        return 1

