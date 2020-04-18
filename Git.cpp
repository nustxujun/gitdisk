#include "Git.h"
#include <chrono>
#include <windows.h>

#if 0
#define LOG(f, l, c) std::cout << f << ":" << l << " " << c << std::endl;
#define CHECK_IF(ret) ([&,this](){auto err = ret;if (err < 0) {LOG(__FILE__, __LINE__, #ret);} return err;})
#define CHECK(ret) Promise()CHECK_IF(ret)
#define DO(ret) ([&,this](){ret;return 0;})
#define CATCH(f) .error([](){f;})
#else
#define CHECK(x) { auto ret = x; if (ret < 0) std::cout << __FILE__ << ":" << __LINE__ << " "<< #x  << " err:" << git_error_last()->message << std::endl;}
#define CHECK_IF(x) CHECK(x);
#define DO(x) x;
#define CATCH(x) 

#endif


Git::Git()
{
	git_libgit2_init();
}

Git::~Git()
{
	git_libgit2_shutdown();
}

void Git::init(const std::string& localpath, const std::string& url, const std::string& usr, const std::string& pwd)
{
	Callbacks::username = usr;
	Callbacks::password = pwd;

	close();
	auto path = localpath;
	if (path.back() == '/' || path.back() == '\\')
		path.pop_back();
	if (std::filesystem::exists(path + "/.git"))
	{
		git_remote* remote = 0;
		CHECK(git_repository_open(&mRepository, path.c_str()))
		DO(sync());
	}
	else if (!url.empty())
	{
		git_clone_options options;
		CHECK(git_clone_init_options(&options, GIT_CLONE_OPTIONS_VERSION))
		DO(options.fetch_opts.callbacks.credentials = Callbacks::credential_acquire_cb)
		CHECK_IF(git_clone(&mRepository,url.c_str(), path.c_str(), NULL));
	}

}

void Git::close()
{
	if (mRepository)
	{
		git_repository_free(mRepository);
		mRepository = NULL;
	}
}

void Git::sync()
{

	git_remote* remote = 0;
	git_index* index = 0;
	git_reference* headref = 0;
	git_commit* headcmt = 0;
	git_reference* head_ref = 0;
	git_oid commit_oid, tree_oid;
	git_signature* signature = 0;
	auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::string time_str = std::ctime(&time);
	git_push_options push_options;
	git_fetch_options fetch_options;
	git_reference* remoteheadref = 0;
	git_commit* remoteheadcmt = 0;


	git_merge_analysis_t analysis;
	git_merge_preference_t preference;

	Callbacks::matchedCount = 0;

	CHECK(git_remote_lookup(&remote, mRepository, "origin"))
	CHECK_IF(git_repository_head(&headref,  mRepository))
	CHECK_IF(git_commit_lookup(&headcmt,mRepository,git_reference_target(headref)))
	CHECK_IF(git_signature_default(&signature, mRepository))
	// stage
	CHECK_IF(git_repository_index(&index, mRepository))
	CHECK_IF(git_index_add_all(index, NULL, GIT_INDEX_ADD_DISABLE_PATHSPEC_MATCH, Callbacks::index_matched_path_cb,NULL));

	git_strarray refs = { 0 };
	int error = git_reference_list(&refs, mRepository);

	if (Callbacks::matchedCount > 0)
	{
		git_tree* tree = 0;
		// commit
		CHECK(git_index_write(index))
		CHECK_IF(git_index_write_tree(&tree_oid, index))
		CHECK_IF(git_tree_lookup(&tree, mRepository, &tree_oid))
		CHECK_IF(git_commit_create_v(
			&commit_oid,
			mRepository,
			"HEAD",
			signature,
			signature,
			NULL,
			time_str.c_str(),
			tree,
			1, headcmt));

		git_tree_free(tree);

	}
	git_annotated_commit* head_commit = 0;
	// fetch
	CHECK(git_fetch_init_options(&fetch_options, GIT_FETCH_OPTIONS_VERSION))
	DO(fetch_options.callbacks.credentials = Callbacks::credential_acquire_cb)
	CHECK_IF(git_remote_fetch(remote, NULL, &fetch_options, "fetch"))
	CHECK_IF(git_reference_lookup(&remoteheadref, mRepository, "FETCH_HEAD"))
	// merge
	CHECK_IF(git_annotated_commit_lookup(&head_commit, mRepository, git_reference_target(remoteheadref)))
	CHECK_IF(git_commit_lookup(&remoteheadcmt, mRepository, git_reference_target(remoteheadref)))
	CHECK_IF(git_merge_analysis(&analysis,&preference,mRepository, (const git_annotated_commit**)&head_commit, 1));

	if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
		
	}
	else
	{
		git_merge_options merge_options;
		git_checkout_options checkout_options;
		CHECK(git_merge_init_options(&merge_options, GIT_MERGE_OPTIONS_VERSION))
		CHECK_IF(git_checkout_init_options(&checkout_options, GIT_CHECKOUT_OPTIONS_VERSION))
		DO(checkout_options.checkout_strategy = GIT_CHECKOUT_FORCE | GIT_CHECKOUT_ALLOW_CONFLICTS)
		CHECK_IF(git_merge(
			mRepository,
			(const git_annotated_commit**)&head_commit, 1,
			&merge_options, &checkout_options))

		;

		if (git_index_has_conflicts(index)) {
			auto ret = ::MessageBoxW(NULL, L"upload or download",NULL, MB_ICONEXCLAMATION | MB_YESNOCANCEL);
			if (ret == IDYES)
			{
				git_checkout_options co_options;
				CHECK(git_checkout_init_options(&co_options, GIT_CHECKOUT_OPTIONS_VERSION))
				CHECK_IF(git_checkout_tree(mRepository,(const git_object*)remoteheadcmt,&co_options))
				;
			}
			else if (ret == IDNO)
			{

			}
			else
			{
				return;
			}
		}
		else 
		{
			git_tree* tree = 0;
			git_commit* remoteheadcmt = 0;
			CHECK(git_repository_index(&index, mRepository))
			CHECK_IF(git_index_write_tree(&tree_oid, index))
			CHECK_IF(git_tree_lookup(&tree, mRepository, &tree_oid))
			CHECK_IF(git_commit_create_v(
				&commit_oid,
				mRepository,
				"HEAD",
				signature, 
				signature,
				NULL, 
				"Merge",
				tree,
				2,headcmt, remoteheadcmt))
			;

			git_tree_free(tree);
		}
	}

	if (remoteheadcmt != headcmt)
	{
		char* refspec = "refs/heads/master";
		const git_strarray refspecs = { &refspec, 1 };
		// push
		CHECK(git_push_options_init(&push_options, GIT_PUSH_OPTIONS_VERSION))
		DO(push_options.callbacks.credentials = &Callbacks::credential_acquire_cb)
		CHECK_IF(git_remote_push(remote, &refspecs, &push_options))
		;
	}
	git_index_free(index);
	git_signature_free(signature);
	git_remote_free(remote);
	git_reference_free(head_ref);
	git_reference_free(remoteheadref);
	git_commit_free(headcmt);
	git_commit_free(remoteheadcmt);
	git_annotated_commit_free(head_commit);

}




std::string Git::Callbacks::username;
std::string Git::Callbacks::password;
size_t Git::Callbacks::matchedCount = 0;