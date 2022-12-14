#include "MidiKeysClient.hpp"
#include "lib/utils.hpp"
#include "lib/log.hpp"

#include <fcntl.h>
#include <linux/input.h>

MidiKeysClient::MidiKeysClient(const char *kbdMapFile, const char *clientName, const char *dstName) : MidiClient(clientName, nullptr, dstName)
{
	auto kbdId = findKbdEvent();
	std::string tmp = "/dev/input/event" + kbdId;

	mFdKbd = open(tmp.c_str(), O_RDONLY);
	if (mFdKbd == -1)
	{
		throw std::runtime_error("Cannot open typing keyboard file: " + tmp);
	}
	LOG(LogLvl::DEBUG) << "Opened typing keyboard, input Id: " << kbdId;
	parse_file(kbdMapFile);
}

void MidiKeysClient::run()
{
	ssize_t n;
	struct input_event kbd_ev;
	while (true)
	{
		n = read(mFdKbd, &kbd_ev, sizeof(kbd_ev));
		if (n == (ssize_t)-1)
		{
			if (errno == EINTR)
			{
				continue;
			}
			throw std::runtime_error("Error reading typing keyboard");
		}
		if (n != sizeof kbd_ev)
			continue;
		if (kbd_ev.type != EV_KEY)
			continue;

		LOG(LogLvl::DEBUG) << "Typing keyboard: " << kbd_ev.value << " " << kbd_ev.code;

		if (kbd_ev.value < 0 || kbd_ev.value > 1)
			continue;
		if (mKbdMap.find((int)kbd_ev.code) == mKbdMap.end())
			continue;

		snd_seq_event_t event;
		snd_seq_ev_clear(&event);
		event.type = SND_SEQ_EVENT_NOTEON;
		event.data.note.channel = 0;
		event.data.note.note = mKbdMap.at((int)kbd_ev.code);
		event.data.note.velocity = kbd_ev.value == 0 ? 0 : 100;

		LOG(LogLvl::DEBUG) << "Send ch:note:vel: " << event.data.note.channel << ":" << event.data.note.note << ":" << event.data.note.velocity;

		send_event(&event);
	}
}

void MidiKeysClient::parse_string(const std::string &s1)
{
	std::string s(s1);
	removeSpaces(s);
	if (s.empty())
		return;

	std::vector<std::string> parts = splitString(s, "=");
	if (parts.size() != 2)
	{
		throw std::runtime_error("Keyboard mapping must have 2 parts: " + s);
	}

	try
	{
		int n1 = std::stoi(parts[0]);
		int n2 = std::stoi(parts[1]);
		LOG(LogLvl::DEBUG) << "Mapping typing key code to note: " << n1 << "=" << n2;
		mKbdMap.insert({n1, n2});
	}
	catch (std::exception &e)
	{
		throw std::runtime_error("Keyboard mapping must have numbers on both sides of '=': " + s);
	}
}

void MidiKeysClient::parse_file(const char *kbdMapFile)
{
	std::ifstream f(kbdMapFile);
	if (!f)
	{
		throw std::runtime_error("Keyboard mapping file not found: " + std::string(kbdMapFile));
	}
	std::string s;
	int k = 0;
	while (getline(f, s))
	{
		try
		{
			k++;
			parse_string(s);
		}
		catch (std::exception &e)
		{
			LOG(LogLvl::ERROR) << "Line: " << k << " in " << kbdMapFile << " Error: "
							   << e.what();
		}
	}
}
