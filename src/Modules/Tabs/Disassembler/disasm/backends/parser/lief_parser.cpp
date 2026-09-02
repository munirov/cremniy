#include "lief_parser.h"

#include <LIEF/LIEF.hpp>
#include <LIEF/ELF.hpp>
#include <LIEF/PE.hpp>
#include <LIEF/MachO.hpp>

namespace {

bool resolveCapstoneArch(const LIEF::Header &header, cs_arch &arch, cs_mode &mode, QString &error)
{
    using ARCHITECTURES = LIEF::Header::ARCHITECTURES;

    const bool is64 = header.is_64();

    switch (header.architecture()) {
    case ARCHITECTURES::X86:
        arch = CS_ARCH_X86;
        mode = is64 ? CS_MODE_64 : CS_MODE_32;
        return true;

    case ARCHITECTURES::X86_64:
        arch = CS_ARCH_X86;
        mode = CS_MODE_64;
        return true;

    case ARCHITECTURES::ARM:
        arch = CS_ARCH_ARM;
        mode = CS_MODE_ARM;
        return true;

    case ARCHITECTURES::ARM64:
        arch = CS_ARCH_ARM64;
        mode = CS_MODE_ARM;
        return true;

    case ARCHITECTURES::MIPS:
        arch = CS_ARCH_MIPS;
        mode = is64 ? CS_MODE_MIPS64 : CS_MODE_MIPS32;
        return true;

    case ARCHITECTURES::PPC:
    case ARCHITECTURES::PPC64:
        arch = CS_ARCH_PPC;
        mode = is64 ? CS_MODE_64 : CS_MODE_32;
        return true;

    case ARCHITECTURES::RISCV:
        arch = CS_ARCH_RISCV;
        mode = is64 ? CS_MODE_RISCV64 : CS_MODE_RISCV32;
        return true;

    default:
        error = QStringLiteral("Unsupported architecture for Capstone backend");
        return false;
    }
}

bool isExecutableSection(const LIEF::Binary &binary, const LIEF::Section &section)
{
    switch (binary.format()) {
    case LIEF::Binary::FORMATS::ELF: {
        const auto &elfSection = static_cast<const LIEF::ELF::Section &>(section);
        return elfSection.has(LIEF::ELF::Section::FLAGS::EXECINSTR);
    }
    case LIEF::Binary::FORMATS::PE: {
        const auto &peSection = static_cast<const LIEF::PE::Section &>(section);
        return peSection.has_characteristic(LIEF::PE::Section::CHARACTERISTICS::MEM_EXECUTE);
    }
    case LIEF::Binary::FORMATS::MACHO: {
        const auto &machoSection = static_cast<const LIEF::MachO::Section &>(section);
        return machoSection.has(LIEF::MachO::Section::FLAGS::SOME_INSTRUCTIONS);
    }
    default:
        return false;
    }
}

} // namespace

ParsedBinary LiefParser::parse(const QString &filePath)
{
    ParsedBinary result;

    std::unique_ptr<LIEF::Binary> binary = LIEF::Parser::parse(filePath.toStdString());
    if (!binary) {
        result.error = QStringLiteral("LIEF failed to parse file: %1").arg(filePath);
        return result;
    }

    const LIEF::Header header = binary->header();

    if (!resolveCapstoneArch(header, result.arch, result.mode, result.error))
        return result;

    if (header.endianness() == LIEF::Header::ENDIANNESS::BIG)
        result.mode = static_cast<cs_mode>(result.mode | CS_MODE_BIG_ENDIAN);

    for (const LIEF::Section &section : binary->sections()) {
        if (!isExecutableSection(*binary, section))
            continue;

        ParsedBinary::Section out;
        out.name = QString::fromStdString(section.name());
        out.vaddr = section.virtual_address();
        out.fileOffset = section.offset();

        const LIEF::span<const uint8_t> content = section.content();
        out.bytes.assign(content.begin(), content.end());

        if (!out.bytes.empty())
            result.sections.push_back(std::move(out));
    }

    if (result.sections.isEmpty())
        result.error = QStringLiteral("No executable sections found in file");

    return result;
}