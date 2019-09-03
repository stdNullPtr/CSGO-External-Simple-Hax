#pragma once

char* EncryptDecrypt(char* str, int size);

#pragma region Memory.hpp

const char* csgoStr = EncryptDecrypt(new char[9]{ "%6+)k)> " }, 8);
const char* clientStr = EncryptDecrypt(new char[20]{ "%)%#+85-(*>'(-h! *" }, 19);
const char* engineStr = EncryptDecrypt(new char[11]{ "#++/+)h! *" }, 10);

const char* gameMustBeRunningStr = "Game must be running!";
const char* unhandledExceptionStr = "Unhandled exception thrown!";
const char* errorWindowCaption = "ERROR";

#pragma endregion

#pragma region Misc

const DWORD SLEEP = 2;
const int PLAYERCNT = 32;

#pragma endregion


#pragma region AimBot

const float MINFOV = 2.0f; //3
const int BONEID = 8;	   //8 - head
const float SMOOTH = 20;   //40

#pragma endregion