#include <tinySPI.h>
// unlike the led85machine, the attiny84 has 12 io pins!
// porting the design to the chip is actually really easy
constexpr auto PWM0 = 8;
constexpr auto PWM1 = 7;
constexpr auto memoryEnable = 3;
constexpr auto framEnable = 2;

// Since I have more SPI enable lines, I can incorporate 256kb of FRAM into the
// design as well. To make sure we run from the FLASH it is "mapped" to 0x40'0000
// and that is where the program starts. The fram is mapped into the first
// 256kbytes of the program space
uint32_t instructionPosition = 0x40'0000;
constexpr uint32_t FlashStartingAddress = 0x40'0000;
constexpr uint32_t instructionMask = 0xFF'FFFF;
bool advanceIp = true;
uint8_t stackPointer = 0;
using GPRContents = uint16_t;
GPRContents registers[16] = { 0 };
GPRContents dataStack[64] = { 0 };
constexpr uint8_t RegisterMask = 0b1111;
constexpr uint8_t StackMask = 0b111111;


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

enum class FlashOpcodes : uint8_t {
	Read = 0x03,
};


//SRAM opcodes
// pins associated with the different spi memory chips
// two spi chips are necessary to hold code space so we have to do some 
// math to figure out which chip to select

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

template<int csPin>
uint16_t read16(uint32_t address) {
  CableSelectHolder<csPin> reader;
  sendOpcode(FlashOpcodes::Read);
  transferAddress(address << 1);
  auto a = static_cast<uint16_t>(SPI.transfer(0x00));
  auto b = static_cast<uint16_t>(SPI.transfer(0x00)) << 8;
  return a | b;
}

auto readSimpleInstruction() noexcept {
	return read32<memoryEnable>(getInstructionPointer());
}

auto load16FromFram(uint32_t address) noexcept {
	return read16<framEnable>(address);
}


template<typename ... Args>
void setupOutputPins(Args&& ... values) noexcept {
	auto fn = [](auto pin) {
		pinMode(pin, OUTPUT);
		digitalWrite(pin, HIGH);
	};
	(fn(values), ...);
}

template<typename ... Args>
void analogWriteValueToAllPins(int value, Args&& ... pins) noexcept {
	(analogWrite(pins, value), ...);
}

void setup() {
	setupOutputPins(memoryEnable, framEnable);
	SPI.begin();
	analogWriteValueToAllPins(0xFF, PWM0, PWM1);
	delay(1000);
	analogWriteValueToAllPins(0x00, PWM0, PWM1);
}
void PWMWrite(int targetPWM, uint8_t value) noexcept {
	analogWrite(targetPWM, value);
}
constexpr auto computeRegisterMask(uint8_t index) noexcept {
	return index & RegisterMask;
}
auto readRegister(uint8_t index) noexcept {
	return registers[computeRegisterMask(index)];
}
void writeRegister(uint8_t index, GPRContents value) noexcept {
	registers[computeRegisterMask(index)] = value;
}
void writeRegister(uint8_t index, uint8_t value) noexcept {
	writeRegister(index, static_cast<GPRContents>(value));
}
void writeRegister(uint8_t index, bool value) noexcept {
	writeRegister(index, value ? 0xFFFF : 0);
}

void pushValueToStack(uint16_t value) noexcept {
	++stackPointer;
	stackPointer &= StackMask;
	dataStack[stackPointer] = value;
}
auto popValueFromStack() noexcept {
	auto value = dataStack[stackPointer];
	--stackPointer;
	stackPointer &= StackMask;
	return value;
}

auto getStackDepth() noexcept {
	return stackPointer;
}

enum class Opcode : uint8_t {
	WritePWM0 = 0,
	DelayMilliseconds,
	JumpAbsolute,
	DelayMicroseconds,
	WritePWM1,
	WritePWM0Indirect,
	WritePWM1Indirect,
	StoreImmediate16IntoRegister,
	OrdinalAdd,
	OrdinalSubtract,
	OrdinalMultiply,
	OrdinalDivide,
	OrdinalRemainder,
	OrdinalShiftLeft,
	OrdinalShiftRight,
	OrdinalMin, 
	OrdinalMax, 
	TransferRegisterContents,
	SwapRegisterContents,
	BitwiseOr,
	BitwiseAnd,
	BitwiseXor,
	InvertBits,
	BitwiseNand,
	BitwiseNor,
	BitwiseXnor,
	Equals,
	NotEquals,
	OrdinalLessThan,
	OrdinalGreaterThan,
	OrdinalLessThanOrEqual,
	OrdinalGreaterThanOrEqual,
	PushToStack,
	PopFromStack,
	GetStackDepth,
};
void loop() {
	auto instruction = readSimpleInstruction();
	auto opcode = static_cast<Opcode>(instruction);
	auto immediate = (instruction & 0xFFFFFF00) >> 8;
	auto register0 = computeRegisterMask(instruction >> 8);
	auto register1 = computeRegisterMask(instruction >> 12);
	auto register2 = computeRegisterMask(instruction >> 16);
	auto register3 = computeRegisterMask(instruction >> 20);
	auto immediate16 = static_cast<GPRContents>(instruction >> 16);
	auto destinationValue = readRegister(register0);
	auto src0Value = readRegister(register1);
	auto src1Value = readRegister(register2);
	auto src2Value = readRegister(register3);
	// chop off the lower 8 bits and use that as a simple instruction
	switch (opcode) {
		case Opcode::WritePWM0:
			PWMWrite(PWM0, immediate);
			break;
		case Opcode::DelayMilliseconds:
			delay(immediate);
			break;
		case Opcode::JumpAbsolute:
			setInstructionPointer(immediate);
			break;
		case Opcode::DelayMicroseconds:
			delayMicroseconds(immediate);
			break;
		case Opcode::WritePWM1:
			PWMWrite(PWM1, immediate);
			break;
		case Opcode::WritePWM0Indirect:
			PWMWrite(PWM0, destinationValue);
			break;
		case Opcode::WritePWM1Indirect:
			PWMWrite(PWM1, destinationValue);
			break;
		case Opcode::StoreImmediate16IntoRegister:
			writeRegister(register0, immediate16);
			break;
		case Opcode::OrdinalAdd:
			writeRegister(register0, src0Value + src1Value);
			break;
		case Opcode::OrdinalSubtract:
			writeRegister(register0, src0Value - src1Value);
			break;
		case Opcode::OrdinalMultiply:
			writeRegister(register0, src0Value * src1Value);
			break;
		case Opcode::OrdinalDivide:
			writeRegister(register0, src0Value / src1Value);
			break;
		case Opcode::OrdinalRemainder:
			writeRegister(register0, src0Value % src1Value);
			break;
		case Opcode::OrdinalShiftLeft:
			writeRegister(register0, src0Value << src1Value);
			break;
		case Opcode::OrdinalShiftRight:
			writeRegister(register0, src0Value >> src1Value);
			break;
		case Opcode::OrdinalMax:
			writeRegister(register0, max(src0Value, src1Value));
			break;
		case Opcode::OrdinalMin:
			writeRegister(register0, min(src0Value, src1Value));
			break;
		case Opcode::TransferRegisterContents:
			writeRegister(register0, src0Value);
			break;
		case Opcode::SwapRegisterContents:
			writeRegister(register0, src0Value);
			writeRegister(register1, destinationValue);
			break;
		case Opcode::InvertBits:
			writeRegister(register0, ~src0Value);
			break;
		case Opcode::BitwiseAnd:
			writeRegister(register0, src0Value & src1Value);
			break;
		case Opcode::BitwiseOr:
			writeRegister(register0, src0Value | src1Value);
			break;
		case Opcode::BitwiseXor:
			writeRegister(register0, src0Value ^ src1Value);
			break;
		case Opcode::BitwiseNand:
			writeRegister(register0, ~(src0Value & src1Value));
			break;
		case Opcode::BitwiseNor:
			writeRegister(register0, ~(src0Value | src1Value));
			break;
		case Opcode::BitwiseXnor:
			writeRegister(register0, ~(src0Value ^ src1Value));
			break;
		case Opcode::Equals:
			writeRegister(register0, src0Value == src1Value);
			break;
		case Opcode::NotEquals:
			writeRegister(register0, src0Value != src1Value);
			break;
		case Opcode::OrdinalGreaterThan:
			writeRegister(register0, src0Value > src1Value);
			break;
		case Opcode::OrdinalLessThan:
			writeRegister(register0, src0Value < src1Value);
			break;
		case Opcode::OrdinalGreaterThanOrEqual:
			writeRegister(register0, src0Value >= src1Value);
			break;
		case Opcode::OrdinalLessThanOrEqual:
			writeRegister(register0, src0Value <= src1Value);
			break;
		case Opcode::PushToStack:
			pushValueToStack(destinationValue);
			break;
		case Opcode::PopFromStack:
			writeRegister(register0, popValueFromStack());
			break;
		case Opcode::GetStackDepth:
			writeRegister(register0, getStackDepth());
			break;
		default:
			instructionPosition = FlashStartingAddress;
			break;
	}
	advanceInstructionPointer();
}
