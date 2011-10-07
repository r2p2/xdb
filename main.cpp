#include <string>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstring>

#include <errno.h>
#include <dirent.h>

void print_error(std::string const& msg, int ferrno = 0)
{
    std::string details;

    std::cerr << msg << std::endl;

    if(!ferrno)
        return;

    switch(ferrno)
    {
    case EACCES: details = "Permission denied.";
        break;
    case EBADF: details = "fd is not a valid file descriptor opened for reading.";
        break;
    case EMFILE: details = "Too many file descriptors in use by process.";
        break;
    case ENFILE: details = "Too many files are currently open in the system.";
        break;
    case ENOENT: details = "Directory does not exist, or name is an empty string.";
        break;
    case ENOMEM: details = "Insufficient memory to complete the operation.";
        break;
    case ENOTDIR: details = "name is not a directory.";
        break;
    default:
        details = "unknown error occured.";
    }

    std::cerr << "Details: " << details << std::endl;
}

struct XDBFile
{
    std::string name;
    unsigned int version;
    std::string up;
    std::string down;

    XDBFile()
    :name()
    ,version(0)
    ,up()
    ,down()
    {
    }

    int parse(std::string const& filename)
    {
        size_t pos_vend = filename.find(".xdb");

        if(pos_vend == std::string::npos)
        {
            print_error("Unable to parse file: " + filename);
            print_error("File ending not found.");
            return 1;
        }

        size_t pos_vstart = filename.find("-");
        if(pos_vend == std::string::npos)
        {
            print_error("Unable to parse file: " + filename);
            print_error("Version separator not found.");
            return 1;
        }

        std::string sversion = filename.substr(pos_vstart+1, pos_vend-pos_vstart-1);
        std::istringstream iss(sversion);
        iss >> version;
        /*if(!iss.good())
        {
            print_error("Unable to parse file: " + filename);
            print_error("Unable to extract version number \""+ sversion +"\" from filename.");
            return 1;
        }*/

        name = filename.substr(0, pos_vstart);

        std::fstream file(filename.c_str(), std::fstream::in);
        if(!file.good())
        {
            print_error("Unable to parse file: " + filename);
            print_error("Unable to open file.");
            return 1;
        }

        // parse state
        // 0 out of any sections
        // 1 in section up
        // 2 in section down
        // 3 finished
        int parse_state = 0;
        std::string line;
        size_t ppos = 0;

        while(parse_state != 3)
        {
            getline(file, line);
            if(!file.eof())
            {
                switch(parse_state)
                {
                case 0:
                    ppos = line.find("begin up");

                    if(ppos == 0)
                    {
                        parse_state = 1;
                        continue;
                    }

                    ppos = line.find("begin down");
                    if(ppos == 0)
                        parse_state = 2;
                    break;
                case 1:
                    ppos = line.find("end up");
                    if(ppos == 0)
                        parse_state = 0;
                    else
                    {
                        up += line + "\n";
                    }
                    break;
                case 2:
                    ppos = line.find("end down");
                    if(ppos == 0)
                        parse_state = 0;
                    else
                    {
                        down += line + "\n";
                    }
                    break;
                }
            }
            else
            {
                parse_state = 3;
            }
        }
        file.close();

        return 0;
    }
};

bool compare(XDBFile const &left, XDBFile const& right)
{
    return left.version < right.version;
}

bool compare_filename_end(std::string const& filename, std::string const &end)
{
    size_t pos = filename.find(end);

    if(pos == std::string::npos || pos != (filename.size() - end.size()))
        return false;

    return true;
}

int db_files_from_dir(std::string const& directory, std::vector<XDBFile> &db_files)
{
    DIR *dir;
    struct dirent *ent;
    std::string const xdbfend = ".xdb";

    dir = opendir(directory.c_str());
    if(!dir)
    {
        print_error("Unable to open directory with database scripts.");
        return 1;
    }

    while((ent = readdir(dir)))
    {
        std::string filename = ent->d_name;

        if(filename == "." || filename == "..")
            continue;

        if(compare_filename_end(filename, xdbfend))
        {
            XDBFile xdbfile;
            if(xdbfile.parse(filename))
            {
                return 1;
            }
            db_files.push_back(xdbfile);
        }
    }

    sort(db_files.begin(), db_files.end(), compare);

    closedir(dir);
    return 0;
}

void start_transaction()
{
    std::cout << "begin transaction;" << std::endl;
}

void end_transaction()
{
    std::cout << "commit;" << std::endl;
}

void rollback()
{
    std::cout << "rollback;" << std::endl;
}


std::vector<XDBFile>::const_iterator iterator_to_version(std::vector<XDBFile> const& files, int version)
{
    std::vector<XDBFile>::const_iterator it = files.begin();
    XDBFile file;

    do {
       file  = *it;
    } while(file.version != version && ++it != files.end());


    return it;
}

void migrate(std::vector<XDBFile> const& files, int source_version, int target_version)
{
    if(source_version == target_version)
        return;

    std::vector<XDBFile>::const_iterator it = iterator_to_version(files, source_version);
    if(it == files.end())
        it = files.begin();
    else
    {
        if(source_version < target_version)
            it++;
        else
            it--;
    }

    while(it!= files.end())
    {
        XDBFile const& file = *it;

        if(source_version < target_version)
        {
            std::cout << "-- " << "(" << file.version << ") " << file.name << std::endl << file.up << std::endl;
            it++;
        }
        else
        {
            std::cout << "-- " << "(" << file.version << ") " << file.name << std::endl << file.down << std::endl;
            it--;
        }

        if(file.version == target_version)
            return;
    }
}

void usage()
{
    exit(1);
}

int create_new_migration(std::string const& name, int version)
{
	std::stringstream filename;
	filename << name  << "-" << version << ".xdb";
	std::fstream file(filename.str().c_str(), std::ios_base::in | std::ios_base::out | std::ios_base::trunc);
    if(!file.good())
    {
        print_error("Unable to open new file: " + filename.str());
        return 1;
    }

	file << "begin up" << std::endl << std::endl << "end up" << std::endl << std::endl;
	file << "begin down" << std::endl << std::endl << "end down" << std::endl;

	file.close();
	return 0;
}

bool is_version_in_file_list(std::vector<XDBFile> const& files, int version)
{
    std::vector<XDBFile>::const_iterator it;
    for(it = files.begin(); it != files.end(); it++)
    {
        XDBFile const& file = *it;
        if(file.version == version)
            return true;
    }
    return false;
}

int validate_source_version(std::vector<XDBFile> const& files, int source_version)
{
	if(source_version == 0)
		return 0;
	else if(source_version < 0)
	{
		std::cerr << "Invalid source version value. Needs to be greater or equal 0." << std::endl;
		return 1;
	}

	if(is_version_in_file_list(files, source_version))
        return 0;
	
	std::cerr << "There is no corresponding migration version to submitted source version." << std::endl;
	return 1;
}

int validate_target_version(std::vector<XDBFile> const& files, int target_version)
{
    if(target_version == 0)
        return 0;
    else if(target_version < 0)
    {
        std::cerr << "Invalid target version value. Needs to be greater or equal 0." << std::endl;
        return 1;
    }

    if(is_version_in_file_list(files, target_version))
        return 0;

    std::cerr << "There is no corresponding migration version to submitted target version." << std::endl;
    return 1;
}


int main(int argc, char** argv)
{
    std::vector<XDBFile> files;
    int source_version = 0;
    int target_version = 0;
	std::string new_migration_name = "";
	bool add_new_migration = false;
    bool dry_run = false;

    for(int argi = 1; argi < argc; argi++)
    {
        if(strcmp(argv[argi], "-s") == 0)
        {
            if(++argi >= argc)
                usage();

            source_version = atoi(argv[argi]);
        }
        else if(strcmp(argv[argi], "-t") == 0)
        {
            if(++argi >= argc)
                usage();

            target_version = atoi(argv[argi]);
        }
        else if(strcmp(argv[argi], "-a") == 0)
		{
            if(++argi >= argc)
                usage();
			add_new_migration = true;
			new_migration_name = argv[argi];
		}
        else if(strcmp(argv[argi], "-d") == 0)
            dry_run = true;
        else
        {
            usage();
        }
    }

    if(db_files_from_dir(".", files))
        return 1;

	if(add_new_migration)
	{
		if(!dry_run && new_migration_name != "")
		{
			int new_version = 1;
			if(!files.empty())
				new_version = files[files.size()-1].version + 1;
			
			create_new_migration(new_migration_name, new_version);
		}
	}	
	else
	{
		if(validate_source_version(files, source_version))
			return 1;
        if(validate_target_version(files, target_version))
            return 1;

		start_transaction();

		if(target_version == 0)
			target_version = files[files.size()-1].version;

		migrate(files, source_version, target_version);

		if(!dry_run)
			end_transaction();
		else
			rollback();
	}
    return 0;
}
