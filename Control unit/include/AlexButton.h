#pragma once

#include <Arduino.h>
#include <digitalWriteFast.h>

class AlexButton
{
private:
	bool _isButtonPressed = false;
	volatile uint8_t _buttonClickedCount = 0;
	volatile bool _isButtonHolded = false;
	volatile long _buttonPressedTime = -1;
	bool _InputPullUp = false;
	int8_t _buttonPinNum = -1;
	uint16_t _holdTimeOutMs = 500;
	uint8_t _debounceTimeOutMs = 50;
	uint16_t _lastStateChangeTime = 0;

public:
	AlexButton(uint8_t buttonPinNumber, bool inputPullUp = true)
	{
		_buttonPinNum = buttonPinNumber;
		_InputPullUp = inputPullUp;
		pinMode(_buttonPinNum, _InputPullUp ? INPUT_PULLUP : INPUT);
	}

	void tick()
	{
		bool isButtonPressed = digitalReadFast(_buttonPinNum);

		if (_InputPullUp)
		{
			isButtonPressed = !isButtonPressed;
		}

		if (isButtonPressed != _isButtonPressed && 
		 	  millis() - _lastStateChangeTime < _debounceTimeOutMs)
		{
			// Serial.println("Deb");
			return;
		}

		long curTime = millis();

		if (isButtonPressed && !_isButtonPressed)//from released to pressed state
		{
			_buttonPressedTime = curTime;
			_lastStateChangeTime = curTime;
			// static uint16_t pressCounter = 1;
			// Serial.print("Pressed: ");
			// Serial.println(pressCounter++);
		}
		else if (!isButtonPressed && _isButtonPressed) // from pressed to released state
		{
			if (_buttonPressedTime != -1 && (curTime - _buttonPressedTime) < _holdTimeOutMs)
			{
				_buttonClickedCount++;
				// Serial.println("Click");
			}
			
			_buttonPressedTime = -1;
			_lastStateChangeTime = curTime;
			// static uint16_t releaseCounter = 1;
			// Serial.print("Released: ");
			// Serial.println(releaseCounter++);
		}
		else//if they are equal, no state change
		{
			if (_buttonPressedTime != -1 && (curTime - _buttonPressedTime) > _holdTimeOutMs)
			{
				_isButtonHolded = true;
			}
		}
		
		_isButtonPressed = isButtonPressed;
	}

	bool isClicked()
	{
		if (_buttonClickedCount > 0)
		{
			_buttonClickedCount--;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool isHolded()
	{
		bool res = _isButtonHolded;

		if (_isButtonHolded)
		{
			_buttonPressedTime = -1;
		}
		
		_isButtonHolded = false;
		return res;
	}

	bool isDown()
	{
		bool isButtonPressed = digitalReadFast(_buttonPinNum);

		if (_InputPullUp)
		{
			isButtonPressed = !isButtonPressed;
		}

		return isButtonPressed;
	}
};