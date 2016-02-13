#OS_Workspace

A repo of your very own!

##Remote Setup:

	git remote add upstream git@github.com:mizzoucs/OSSP16_Assignments.git

##Remote Sync:

Check for updates often! These updates will contain future assignments, milestones, example code, and bugfixes. **_Keeping your repository up to date is your responsibility._**

	git fetch upstream
	git merge upstream

##Additional Git Resources:

* [Branch and Merge Docs](https://git-scm.com/book/en/v2/Git-Branching-Basic-Branching-and-Merging)
* [Tag Docs](https://git-scm.com/book/en/v2/Git-Basics-Tagging)
* [Google](https://google.com)

##OS_Library!

	git clone git@github.com:mizzoucs/OS_Library.git
	cd OS_Library && mkdir build && cd build
	cmake .. && sudo make install && sudo ldconfig

##Submission:

Tags will be used to track and download your submissions for grading. For an overview of tags, check out the [git tag docs](https://git-scm.com/book/en/v2/Git-Basics-Tagging). To submit, you will need to have the submission tag for the assignment/milestone. The submission tag can most likely be found on the assignment document or the project readme.

Tags are neither automagically created nor uploaded to GitHub, so remember to create and submit yours before the due date! Creating a tag is super easy, just run

	git tag -a <TAG> -m <MESSAGE>

to create a tag of your current commit, and push it to github with

	git push origin <TAG>

###/!\ WARNING /!\

Be wary of force pushing! Changing your commit history can be dangerous, especially when working in teams. Rewriting history that has already been submitted may cause future submissions to be rejected!


##Updating Your Submission:

If you want to update your tagged version, first delete old tag with

	git tag -d <TAG>
	
If you have already pushed your tag to GitHub, you must also delete the uploaded tag with

	git push origin :refs/tags/<TAG>
	
After the existing tag has been deleted, the new tag can be made and pushed like before.