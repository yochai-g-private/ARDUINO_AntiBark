#include "StdIR.h"
#include "Logger.h"
#include "RGB_Led.h"
#include "Timer.h"
#include "Random.h"
#include "Cfg.h"

enum    Pins
{
    IR_PIN          = 2,
    LED_RED         = 4,
    LED_GREEN       = 6,
    LED_BLUE        = 8,
    IR_MENU_LED_PIN = 10,
    SPEAKER_PIN     = 12,
};

StdIR::Receiver     IR(IR_PIN);
RGB_Led             RGB(LED_RED, LED_GREEN, LED_BLUE);
DigitalOutputPin    IR_menu_led(IR_MENU_LED_PIN);
ToneOutputPin       speaker(SPEAKER_PIN);
Timer               timer;
Timer               IR_mode_timer;
#define             IR_mode     IR_mode_timer.IsStarted()

void restore_to_factory_defaults();
void switch_to_functional_mode();
void start_timer();
void update_leds();
void toggle_speaker();
void check_IR();
void load();
void store();

enum 
{
    __MIN_FREQUENCY_kHz = 15,
    __MAX_FREQUENCY_kHz = 65,
    FRQUENCY_STEP_kHz   = 5,

    DEFAULT_MAX_TIME_INTERVAL_SECONDS = 30,
};

uint32_t    MIN_TIME_INTERVAL_SECONDS =  1,
            MAX_TIME_INTERVAL_SECONDS;

uint32_t    MIN_FREQUENCY_kHz,
            MAX_FREQUENCY_kHz;

class MyCfg : public Cfg
{
protected:

    MyCfg() : Cfg("Anti-Bark", 1)   {   }

    class Frequency : public Leaf
    {
    public:

        Frequency(const char* _name, uint32_t& _value) : name(_name), value(_value)     {   }

        const char* GetName()       const   { return name; }

        bool SetValue(String& s)            { return convert(s.c_str(), value); }

        uint32_t getSize()                              const   { return sizeof(value); }
        void*    getData()                              const   { return &value; }
        String   ToString()                             const   { return Cfg::Leaf::ToString((unsigned long&)value); }

        const char* name;
        uint32_t&   value;
    };

    static Frequency    lowest_freq;
    static Frequency    highest_freq;
    static Frequency    interval;

    class MyRoot : public Root
    {
        DECLARE_CFG_NODE_ITERATOR_FUNCS_3(lowest_freq, highest_freq, interval);
    };

    MyRoot root;

    Root& GetRoot()
    {
        return root;
    }

public:

    static MyCfg instance;
};

MyCfg::Frequency MyCfg::lowest_freq ("lowest",   MIN_FREQUENCY_kHz);
MyCfg::Frequency MyCfg::highest_freq("highest",  MAX_FREQUENCY_kHz);
MyCfg::Frequency MyCfg::interval    ("interval", MAX_TIME_INTERVAL_SECONDS);

MyCfg            MyCfg::instance;

//==========================================================
void setup() 
{
    Logger::Initialize();

    RGB.GetRed().On();          delay(1000); RGB.SetOff();
    RGB.GetGreen().On();        delay(1000); RGB.SetOff();
    RGB.GetBlue().On();         delay(1000); RGB.SetOff();
    IR_menu_led.On();           delay(1000); IR_menu_led.Off();

    restore_to_factory_defaults();

    load();

    IR.Begin();

    switch_to_functional_mode();

    LOGGER << "Ready!" << NL;
}
//==========================================================
void loop() 
{
    if(IR_mode_timer.Test())
    {
        switch_to_functional_mode();
        return;
    }

    toggle_speaker();
    check_IR();
}
//==========================================================
void restore_to_factory_defaults()
{
    MIN_FREQUENCY_kHz         = __MIN_FREQUENCY_kHz + FRQUENCY_STEP_kHz;
    MAX_FREQUENCY_kHz         = __MAX_FREQUENCY_kHz;
    MAX_TIME_INTERVAL_SECONDS = DEFAULT_MAX_TIME_INTERVAL_SECONDS;
}
//==========================================================
void set_IR_mode(bool on)
{
    if(on)
    {
        if(IR_menu_led.IsOff())
        {
            LOGGER << "IR mode ON" << NL;
            IR_menu_led.On();
        }

        IR_mode_timer.StartOnce(30 * 1000);
    }
    else
    {
        if(IR_menu_led.IsOn())
        {
            LOGGER << "IR mode OFF" << NL;
            IR_menu_led.Off();
            IR_mode_timer.Stop();
        }
    }
}
//==========================================================
void switch_to_functional_mode()
{
    MyCfg::instance.Show();

    speaker.Quiet();
    timer.StartOnce(1);

    set_IR_mode(false);

    toggle_speaker();
}
//==========================================================
#define get_random(minval, maxval)  ((Random::Get() % (maxval - minval + 1)) + minval)
//==========================================================
void start_timer()
{
    uint32_t seconds = get_random(MIN_TIME_INTERVAL_SECONDS, MAX_TIME_INTERVAL_SECONDS);
    timer.StartOnce(seconds * 1000);
    LOGGER << "Timer triggered for " << seconds << " seconds" << NL;
}
//==========================================================
void update_leds()
{
    RGB.SetOff();
    IDigitalOutput& led = (speaker.IsQuiet()) ? RGB.GetBlue() : RGB.GetGreen();
    led.On();
}
//==========================================================
void play_sound(uint32_t freq_kHz)
{
    speaker.Tone(freq_kHz * 1000); 
    LOGGER << "Playing tone " << freq_kHz << " kHz" << NL;
}
//==========================================================
void toggle_speaker()
{
    if(IR_mode)
        return;

    if(!timer.Test())
        return;

    if(speaker.IsQuiet())
    {
        uint32_t frequency = get_random(MIN_FREQUENCY_kHz, MAX_FREQUENCY_kHz);
        play_sound(frequency);
    }
    else
    {
        speaker.Quiet();
        LOGGER << "Silent" << NL;
    }

    update_leds();
    start_timer();
}
//==========================================================
void check_IR()
{
    StdIR::Key ir_key;
    
    if(!IR.Recv(ir_key))
        return;

    bool proceed_IR_key(StdIR::Key ir_key);

    bool Ok = proceed_IR_key(ir_key);

    if(IR_mode)
    {
        IDigitalOutput& led = (Ok) ? RGB.GetGreen() : RGB.GetRed();

        led.On();
        delay(250);
        led.Off();

        set_IR_mode(true);

        return;
    }

    switch_to_functional_mode();
}
//==========================================================
enum IR_STATUS
{
    MAIN,
    SET_LOWEST,
    SET_HIGHEST,
    FACTORY_RESTORE,
};

IR_STATUS     ir_status;
//==========================================================
bool change_frequency(uint32_t& freq, int add, uint32_t margin)
{
    add *= FRQUENCY_STEP_kHz;

    LOGGER << "freq=" << freq << ", margin=" << margin << ", add=" << add << NL;
    if(margin == (freq + add))
        return false;

    freq += add;

    LOGGER << (ir_status == SET_LOWEST ? "Lowest" : "Highest") << " frequency set to " << freq << " kHz" << NL;

    play_sound(freq);
    store();

    return true;   
}
//==========================================================
bool set_max_time_interval(uint32_t seconds)
{
    MAX_TIME_INTERVAL_SECONDS = seconds;
    store();
    LOGGER << "Max. time interval set to " << seconds << " seconds" << NL;
    return true;
}
//==========================================================
bool proceed_IR_key(StdIR::Key ir_key)
{
    static bool human = false;

    LOGGER << "IR key: " << StdIR::GetName(ir_key) << NL;
    
    if(!IR_mode)
    {
        if(StdIR::OK == ir_key)
        {
            speaker.Quiet();
            RGB.SetOff();
            timer.Stop();

            set_IR_mode(true);
            
            ir_status   = MAIN;
            human       = false;

            return true;
        }

        return false;
    }

    switch(ir_key)
    {
        case StdIR::OK      : 
            switch(ir_status)
            {
                case MAIN           :   set_IR_mode(false);
                                        return true;
                case SET_LOWEST     :   
                case SET_HIGHEST    :   
                case FACTORY_RESTORE: speaker.Quiet(); 
                                        ir_status = MAIN;   
                                        return true;
            }

            return false;

        case StdIR::LEFT        :   ir_status = SET_LOWEST;
                                    play_sound(MIN_FREQUENCY_kHz);
                                    return true;

        case StdIR::RIGHT       :   ir_status = SET_HIGHEST;
                                    play_sound(MAX_FREQUENCY_kHz);
                                    return true;
        
        case StdIR::UP          :   
            switch(ir_status)
            {
                case SET_LOWEST     :   return change_frequency(MIN_FREQUENCY_kHz, 1, MAX_FREQUENCY_kHz);
                case SET_HIGHEST    :   return change_frequency(MAX_FREQUENCY_kHz, 1, __MAX_FREQUENCY_kHz);
            }
            
            return false;

        case StdIR::DOWN        :   
            switch(ir_status)
            {
                case SET_LOWEST     :   return change_frequency(MIN_FREQUENCY_kHz, -1, __MIN_FREQUENCY_kHz);
                case SET_HIGHEST    :   return change_frequency(MAX_FREQUENCY_kHz, -1, MIN_FREQUENCY_kHz);
            }
            
            return false;

        case StdIR::STAR        :
            switch(ir_status)
            {
                case MAIN           :   ir_status = FACTORY_RESTORE; 
                                        return true;
                case FACTORY_RESTORE:   ir_status = MAIN; 
                                        return true;
            }
            
            return false;
            
        case StdIR::DIEZ        :
            switch(ir_status)
            {
                case MAIN           :   human = !human; 
                                        
                                        if(human)
                                            speaker.Tone(440);
                                        else
                                            speaker.Quiet();

                                        return true;

                case FACTORY_RESTORE:   restore_to_factory_defaults();
                                        store();
                                        LOGGER << "Restored to factory defaults" << NL;
                                        ir_status = MAIN;
                                        return true;
            }
            
            return false;

        case StdIR::N0          :   return set_max_time_interval(DEFAULT_MAX_TIME_INTERVAL_SECONDS);

        #define TREATE_NUMKEY( n )  case StdIR::N##n : return set_max_time_interval( n * 10 )

        TREATE_NUMKEY( 1 );
        TREATE_NUMKEY( 2 );
        TREATE_NUMKEY( 3 );
        TREATE_NUMKEY( 4 );
        TREATE_NUMKEY( 5 );
        TREATE_NUMKEY( 6 );
        TREATE_NUMKEY( 7 );
        TREATE_NUMKEY( 8 );
        TREATE_NUMKEY( 9 );

        #undef  TREATE_NUMKEY
    }
}
//==========================================================
void load()             { MyCfg::instance.Load(); }
//==========================================================
void store()            { MyCfg::instance.Store(); }
//==========================================================
