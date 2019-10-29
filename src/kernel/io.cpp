#include "io.h"

static size_t ReadLineFromConsole(char *buffer, size_t bufferSize)
{
	size_t pos = 0;
	while (pos < bufferSize)
	{
		kiv_hal::TRegisters registers;
		registers.rax.h = static_cast<uint8_t>(kiv_hal::NKeyboard::Read_Char);
		kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Keyboard, registers);
		
		if (!registers.flags.non_zero)
		{
			// nic jsme nepřečetli
			// pokud je rax.l EOT, pak byl zřejmě vstup korektně ukončen
			// jinak zřejmě došlo k chybě zařízení
			break;
		}

		char ch = registers.rax.l;

		// ošetříme známé kódy
		switch (static_cast<kiv_hal::NControl_Codes>(ch))
		{
			case kiv_hal::NControl_Codes::BS:
			{
				// mažeme znak z bufferu
				if (pos > 0)
				{
					pos--;

					registers.rax.h = static_cast<uint8_t>(kiv_hal::NVGA_BIOS::Write_Control_Char);
					registers.rdx.l = ch;
					kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);
				}

				break;
			}
			case kiv_hal::NControl_Codes::LF:
			{
				// jenom pohltíme, ale nečteme
				break;
			}
			case kiv_hal::NControl_Codes::NUL:  // chyba čtení?
			case kiv_hal::NControl_Codes::CR:  // dočetli jsme až po Enter
			{
				return pos;
			}
			default:
			{
				buffer[pos] = ch;
				pos++;

				registers.rax.h = static_cast<uint8_t>(kiv_hal::NVGA_BIOS::Write_Control_Char);
				registers.rdx.l = ch;
				kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);

				break;
			}
		}
	}

	return pos;

}

void Handle_IO(kiv_hal::TRegisters & context)
{
	switch (static_cast<kiv_os::NOS_File_System>(context.rax.l))
	{
		case kiv_os::NOS_File_System::Open_File:
		{
			// TODO
			break;
		}
		case kiv_os::NOS_File_System::Write_File:
		{
			// TODO
			kiv_hal::TRegisters registers;
			registers.rax.h = static_cast<uint8_t>(kiv_hal::NVGA_BIOS::Write_String);
			registers.rdx.r = context.rdi.r;
			registers.rcx.r = context.rcx.r;

			// překlad parametrů dokončen, zavoláme službu
			kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);

			// VGA BIOS nevrací počet zapsaných znaků, takže předpokládáme, že zapsal všechny
			context.rax = registers.rcx;
			context.flags.carry = 0;

			break;
		}
		case kiv_os::NOS_File_System::Read_File:
		{
			// TODO
			context.rax.r = ReadLineFromConsole(reinterpret_cast<char*>(context.rdi.r), context.rcx.r);

			break;
		}
		case kiv_os::NOS_File_System::Seek:
		{
			// TODO
			break;
		}
		case kiv_os::NOS_File_System::Close_Handle:
		{
			// TODO
			break;
		}
		case kiv_os::NOS_File_System::Delete_File:
		{
			// TODO
			break;
		}
		case kiv_os::NOS_File_System::Set_Working_Dir:
		{
			// TODO
			break;
		}
		case kiv_os::NOS_File_System::Get_Working_Dir:
		{
			// TODO
			break;
		}
		case kiv_os::NOS_File_System::Create_Pipe:
		{
			// TODO
			break;
		}
	}
}
