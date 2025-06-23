#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>

#define OLED_ADDR (0x3C << 1)
#define SENSOR_ADDR (0x40 << 1)

typedef struct {
    int temp;
    int humid;
} SensorData;

typedef struct {
    uint8_t msb, lsb, crc;
} SensorReading;

// Improved font definition with clear degree symbol
const uint8_t font5x7[][5] = {
    // Numbers 0-9
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9

    // Letters A-Z
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z

    // Special characters
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x06,0x09,0x09,0x06,0x00}  // ° (degree symbol)
};

void I2C_Init() {
    TWSR = 0;
    TWBR = 0x48;
}

void I2C_Start() {
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

void I2C_Stop() {
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
    _delay_us(100);
}

void I2C_Write(uint8_t data) {
    TWDR = data;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

uint8_t I2C_Read_ACK() {
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}

uint8_t I2C_Read_NACK() {
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}

void OLED_Command(uint8_t cmd) {
    I2C_Start();
    I2C_Write(OLED_ADDR);
    I2C_Write(0x00);
    I2C_Write(cmd);
    I2C_Stop();
}

void OLED_Data(uint8_t data) {
    I2C_Start();
    I2C_Write(OLED_ADDR);
    I2C_Write(0x40);
    I2C_Write(data);
    I2C_Stop();
}

void OLED_Init() {
    _delay_ms(100);
    uint8_t init_seq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6,
        0xAF
    };
    uint8_t i;
    for(i = 0; i < sizeof(init_seq); i++) {
        OLED_Command(init_seq[i]);
        _delay_ms(5);
    }
}

void OLED_SetCursor(uint8_t row, uint8_t col) {
    OLED_Command(0xB0 + row);
    OLED_Command(col & 0x0F);
    OLED_Command(0x10 + (col >> 4));
}

void OLED_Clear() {
    uint8_t page;
    for(page = 0; page < 8; page++) {
        OLED_SetCursor(page, 0);
        uint8_t col;
        for(col = 0; col < 128; col++) {
            OLED_Data(0x00);
        }
    }
}

void OLED_PrintChar(char c) {
    uint8_t index = 0;

    if(c == ' ') {
        uint8_t i;
        for(i = 0; i < 6; i++) OLED_Data(0x00);
        return;
    }
    else if(c >= '0' && c <= '9') {
        index = c - '0';
    }
    else if(c >= 'A' && c <= 'Z') {
        index = 10 + (c - 'A');
    }
    else if(c == ':') {
        index = 36;
    }
    else if(c == '%') {
        index = 37;
    }
    else if(c == '°') {
        index = 38;
    }

    // Print the character
    uint8_t i;
    for(i = 0; i < 5; i++) {
        OLED_Data(font5x7[index][i]);
    }
    OLED_Data(0x00); // Column padding
}

void OLED_PrintString(const char *str) {
    while(*str) {
        OLED_PrintChar(*str++);
    }
}

uint8_t CalcCRC(uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    uint8_t poly = 0x31;

    uint8_t i;
    for(i = 0; i < len; i++) {
        crc ^= data[i];
        uint8_t j;
        for(j = 0; j < 8; j++) {
            if(crc & 0x80) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void ReadSensor(SensorData *data) {
    SensorReading reading;

    // Read temperature
    I2C_Start();
    I2C_Write(SENSOR_ADDR);
    I2C_Write(0xF3);
    I2C_Stop();
    _delay_ms(50);

    I2C_Start();
    I2C_Write(SENSOR_ADDR | 0x01);
    reading.msb = I2C_Read_ACK();
    reading.lsb = I2C_Read_ACK();
    reading.crc = I2C_Read_NACK();
    I2C_Stop();

    if(CalcCRC(&reading.msb, 2) == reading.crc) {
        data->temp = (int)(-46.85 + (175.72 * ((reading.msb << 8) | reading.lsb) / 65536.0));
    }

    // Read humidity
    I2C_Start();
    I2C_Write(SENSOR_ADDR);
    I2C_Write(0xF5);
    I2C_Stop();
    _delay_ms(50);

    I2C_Start();
    I2C_Write(SENSOR_ADDR | 0x01);
    reading.msb = I2C_Read_ACK();
    reading.lsb = I2C_Read_ACK();
    reading.crc = I2C_Read_NACK();
    I2C_Stop();

    if(CalcCRC(&reading.msb, 2) == reading.crc) {
        data->humid = (int)(-6.0 + (125.0 * ((reading.msb << 8) | reading.lsb) / 65536.0));
    }
}

int main(void) {
    SensorData sensor;
    char value_str[10];

    I2C_Init();
    OLED_Init();
    OLED_Clear();

    // Display static labels
    OLED_SetCursor(0, 0);
    OLED_PrintString(" DARJA E HARARAT:");
    OLED_SetCursor(2, 0);
    OLED_PrintString(" NAMI");

    while(1) {
        ReadSensor(&sensor);

        // Display temperature
        OLED_SetCursor(1, 0);
        itoa(sensor.temp, value_str, 10);
        OLED_PrintString(" ");
        OLED_PrintString(value_str);
        OLED_PrintChar('°');
        OLED_PrintChar('C');

       // Display humidity
        OLED_SetCursor(3, 0);
        itoa(sensor.humid, value_str, 10);
        OLED_PrintString(" ");
        OLED_PrintString(value_str);
        OLED_PrintChar('%');

        _delay_ms(2000); // Update every 2 seconds
    }
}
