#include "Logger.h"


Logger::Logger() 
{

}

void Logger::log_no_ln(String message)
{
    char buff [20];
    sprintf(buff, "%03lu.%03lu ", millis() / 1000, millis() % 1000);
    String output = String() + buff + message;
    this->print(output);
    Serial.print(output);
}

void Logger::log(String message)
{
    this->log_no_ln(message);
    this->ln();
}

void Logger::ln()
{
    this->print("\n");
}

void Logger::println(String message)
{
    this->print(message);
    this->ln();
}

void Logger::print(String message)
{
    unsigned int shrinkSize = LOGGER_BUFFER_SIZE / 3;

    if(message.length() + 1 > shrinkSize) 
        return;

    if(this->position + message.length() + 1 >= LOGGER_BUFFER_SIZE){
        memmove(this->buffer, this->buffer + shrinkSize, LOGGER_BUFFER_SIZE - shrinkSize);
        //strcpy(this->buffer, this->buffer + shrinkSize);
        this->position = this->position - shrinkSize; 
    }

    //strncpy((char*)this->buffer + this->position, message.c_str(), message.length());
    sprintf((char*)this->buffer + this->position, "%s", message.c_str());
    this->position += message.length();
}


