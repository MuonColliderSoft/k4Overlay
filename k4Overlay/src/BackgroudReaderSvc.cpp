#include "BackgroudReaderSvc.h"
#include <random>

StatusCode BackgroundReaderSvc::initialize()
{
    std::random_device r_device;
    std::mt19937 r_generator(r_device());
    vector<string> s_filenames;

    for (auto item : m_filenames) s_filenames.push_back(item);
    if (s_filenames.size() > 1) std::shuffle(s_filenames.begin(), s_filenames.end(), r_generator);
    if (s_filenames.empty())
    {
        error() << "No file names specified" << endmsg;
        return StatusCode::FAILURE;
    }

    m_reader.openFiles(s_filenames);
    total_evns = m_reader.getEntries("events");
    if (total_evns == 0)
    {
        error() << "No events found" << endmsg;
        return StatusCode::FAILURE;
    }

    std::uniform_int_distribution<unsigned> uni_distro { 0, total_evns - 1 };
    curr_evn = uni_distro(r_generator);
    return StatusCode::SUCCESS;
}

unsigned BackgroundReaderSvc::size()
{
    return total_evns;
}

OptFrame BackgroundReaderSvc::getEventFrame()
{
    auto r_frame = m_reader.readEntry("events", curr_evn);
    if (r_frame == nullptr) return std::nullopt;

    curr_evn = curr_evn == total_evns ? curr_evn + 1 : 0;

    return podio::Frame(std::move(r_frame));
}

StatusCode BackgroundReaderSvc::finalize()
{
    return StatusCode::SUCCESS;
}
