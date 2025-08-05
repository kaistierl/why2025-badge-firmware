idf.py build;
PROJECT_NAME=badgehub_dev
LOCAL_ELF_NAME=badgehub.elf
#LOCAL_ELF_NAME=sdl_test.elf
REMOTE_ELF_NAME=hello.elf
#BADGEHUB_PROJECT_API_TOKEN=TODO_EXPORT_IN_ENV_MANUALLY

curl -X POST -H "badgehub-api-token: ${BADGEHUB_PROJECT_API_TOKEN}" \
-F "file=@./build/app_elfs/${LOCAL_ELF_NAME}" \
https://badge.why2025.org/api/v3/projects/${PROJECT_NAME}/draft/files/${REMOTE_ELF_NAME} \
&& echo "did upload badgehub.elf to project ${PROJECT_NAME}"

curl -X 'PATCH' -H "badgehub-api-token: ${BADGEHUB_PROJECT_API_TOKEN}" \
  "https://badge.why2025.org/api/v3/projects/${PROJECT_NAME}/publish" \
  -H 'accept: application/json' \
  -H 'Content-Type: application/json' \
&& echo "did publish"