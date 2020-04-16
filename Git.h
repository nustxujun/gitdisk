#pragma once 
#include "git2.h"
#include <string>
#include <filesystem>
#include <functional>
#include <iostream>
class Git
{
	class Promise
	{
	public:
		Promise& operator()(std::function<int()>&& f)
		{
			mFunctions.emplace_back(f);
			return *this;
		}

		~Promise()
		{
			for (auto& f: mFunctions)
				if (f() < 0)
				{
					auto err = git_error_last(); 
					std::cout << err->message << std::endl;
					if(mFailure)
						mFailure();
				}
		}

		void error(std::function<void()>&& f)
		{
			mFailure = f;
		}
	private:
		std::vector<std::function<int()>> mFunctions;
		std::function<void()> mFailure;
	};
public :
	Git();
	~Git();

	void init(const std::string& localpath, const std::string& url , const std::string& usr, const std::string& pwd );
	void close();
	void sync();
private:
	git_repository* mRepository{nullptr};

	struct Callbacks
	{
		static std::string username;
		static std::string password;

		//git_credential_acquire_cb
		static int credential_acquire_cb(
			git_credential** out,
			const char* url,
			const char* username_from_url,
			unsigned int allowed_types,
			void* payload)
		{
			return git_cred_userpass_plaintext_new(out,username.c_str(), password.c_str());
		}

		static size_t matchedCount;
		//git_index_matched_path_cb
		static int index_matched_path_cb(const char* path, const char* matched_pathspec, void* payload)
		{
			matchedCount++;
			return 0;
		}
	};
};