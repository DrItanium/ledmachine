#include <tinySPI.h>
constexpr auto PWM0 = 4;

uint32_t instructionPosition = 0;
constexpr uint32_t instructionMask = 0xFF'FFFF;
bool advanceIp = true;


uint32_t getInstructionPointer() noexcept {
	return instructionPosition & instructionMask;
}
void setInstructionPointer(uint32_t address) noexcept {
	instructionPosition = address & instructionMask;
	advanceIp = false;
}
void advanceInstructionPointer() noexcept {
	if (advanceIp) {
		++instructionPosition;
	}
	advanceIp = true;
}
template<int pin>
struct CableSelectHolder {
   CableSelectHolder() {
       digitalWrite(pin, LOW);
   }
   ~CableSelectHolder() {
       digitalWrite(pin, HIGH);
   }
};

/*
enum class FRAMOpcodes : uint8_t {
  WREN = 0b00000110,
  WRDI = 0b00000100,
  RDSR = 0b00000101,
  WRSR = 0b00000001,
  READ = 0b00000011,
  FSTRD = 0b00001011,
  WRITE = 0b00000010,
  SLEEP = 0b10111001,
  RDID = 0b10011111,
};
*/

enum class FlashOpcodes : uint8_t {
	Read = 0x03,
};


//SRAM opcodes
// pins associated with the different spi memory chips
// two spi chips are necessary to hold code space so we have to do some 
// math to figure out which chip to select
constexpr auto memoryEnable = 3;

template<typename T> void sendOpcode(T opcode) {
  SPI.transfer(uint8_t(opcode));
}
void transferAddress(uint32_t address) {
  SPI.transfer(uint8_t(address >> 16));
  SPI.transfer(uint8_t(address >> 8));
  SPI.transfer(uint8_t(address));
}

// this does mean we have access to 64 megabytes of space since we become a
// 26-bit memory space
template<int csPin>
uint32_t read32(uint32_t address) {
  CableSelectHolder<csPin> reader;
  sendOpcode(FlashOpcodes::Read);
  transferAddress(address << 2);
  auto a = static_cast<uint32_t>(SPI.transfer(0x00));
  auto b = static_cast<uint32_t>(SPI.transfer(0x00)) << 8;
  auto c = static_cast<uint32_t>(SPI.transfer(0x00)) << 16;
  auto d = static_cast<uint32_t>(SPI.transfer(0x00)) << 24;
  return a | b | c | d;
}

uint16_t readSimpleInstruction() noexcept {
	return read32<memoryEnable>(getInstructionPointer());
}



void setup() {
	pinMode(memoryEnable, OUTPUT);
	digitalWrite(memoryEnable, HIGH);
	SPI.begin();
	analogWrite(PWM0, 0xFF);
	delay(1000);
	analogWrite(PWM0, 0x00);
}

void loop() {
	auto instruction = readSimpleInstruction();
	auto opcode = static_cast<byte>(instruction);
	auto immediate = (instruction & 0xFFFFFF00) >> 8;
	// chop off the lower 8 bits and use that as a simple instruction
	switch (opcode) {
		case 0:
			analogWrite(PWM0, static_cast<byte>(immediate));
			break;
		case 1: // delay
			delay(immediate);
			break;
		case 2: // jump
			setInstructionPointer(immediate);
			break;
		default:
			instructionPosition = 0;
			break;
	}
	advanceInstructionPointer();
}
