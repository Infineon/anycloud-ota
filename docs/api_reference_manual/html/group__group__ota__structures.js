var group__group__ota__structures =
[
    [ "cy_ota_storage_write_info_t", "structcy__ota__storage__write__info__t.html", [
      [ "total_size", "structcy__ota__storage__write__info__t.html#a23215e611d8933d75fc52e902d3f4502", null ],
      [ "offset", "structcy__ota__storage__write__info__t.html#a0405b3806c0de934f5f5f55ac693c184", null ],
      [ "buffer", "structcy__ota__storage__write__info__t.html#af66dba2bcbb117c85bcf20fa0357c525", null ],
      [ "size", "structcy__ota__storage__write__info__t.html#a20c92fe4a9bba891efd3319efbd05060", null ],
      [ "packet_number", "structcy__ota__storage__write__info__t.html#abe05546ecff7e5985d268248c0ebb3e5", null ],
      [ "total_packets", "structcy__ota__storage__write__info__t.html#a3d5cf26f262a52569a71a8948df60b56", null ]
    ] ],
    [ "cy_ota_http_params_t", "structcy__ota__http__params__t.html", [
      [ "server", "structcy__ota__http__params__t.html#a45626624140f47a3b1ec6972d54f86b8", null ],
      [ "file", "structcy__ota__http__params__t.html#a1e685fc8622e19d7a9cd87dc4b159656", null ],
      [ "credentials", "structcy__ota__http__params__t.html#afefcfe7b24dfcf382ceef3fdb6e5b59f", null ]
    ] ],
    [ "cy_ota_mqtt_params_t", "structcy__ota__mqtt__params__t.html", [
      [ "awsIotMqttMode", "structcy__ota__mqtt__params__t.html#a426a5dcd872c5e7aa8f1e5b5a812dc41", null ],
      [ "pIdentifier", "structcy__ota__mqtt__params__t.html#ad34d95200fc95de6118ec4e3d699afd6", null ],
      [ "numTopicFilters", "structcy__ota__mqtt__params__t.html#ac806ee0d85cccc193de4b7b9be601477", null ],
      [ "pTopicFilters", "structcy__ota__mqtt__params__t.html#a7bb69c553cb9c13d8f32b3cb196ad0f9", null ],
      [ "session_type", "structcy__ota__mqtt__params__t.html#a8c08bc782786d64fcf57aa94e3d446cd", null ],
      [ "broker", "structcy__ota__mqtt__params__t.html#ae9c8e8f863362ed79fc130343418605d", null ],
      [ "credentials", "structcy__ota__mqtt__params__t.html#ac479b12c36411127a03f464ad00a0ce8", null ]
    ] ],
    [ "cy_ota_cb_struct_t", "structcy__ota__cb__struct__t.html", [
      [ "reason", "structcy__ota__cb__struct__t.html#ae28ffd179023e8902cbd3e88785540ad", null ],
      [ "cb_arg", "structcy__ota__cb__struct__t.html#afa47e10a9fe4712558dded6625a3c484", null ],
      [ "state", "structcy__ota__cb__struct__t.html#a48361e091a9b49cc103b9b37d7328784", null ],
      [ "error", "structcy__ota__cb__struct__t.html#acc8a4e04f407ca0bbf0dc35e7e42f0c1", null ],
      [ "storage", "structcy__ota__cb__struct__t.html#aafd5022e59a7d19d6a103f9217710644", null ],
      [ "total_size", "structcy__ota__cb__struct__t.html#ad29859423d69a8e0815a833a70284bdb", null ],
      [ "bytes_written", "structcy__ota__cb__struct__t.html#ab43108a89dadf59dcad71d4b4c75411d", null ],
      [ "percentage", "structcy__ota__cb__struct__t.html#a1d802b856542ce6b7e59e2559026307d", null ],
      [ "connection_type", "structcy__ota__cb__struct__t.html#aabc069ca938caaeda44300df68d31903", null ],
      [ "broker_server", "structcy__ota__cb__struct__t.html#ac0bd6583f60af5fe056173f89ee5e0e6", null ],
      [ "credentials", "structcy__ota__cb__struct__t.html#a5eb663dbca52197e49b911f2f5e05af9", null ],
      [ "http_connection", "structcy__ota__cb__struct__t.html#aa77c36705ebc2de5ebfc6e05e6249981", null ],
      [ "mqtt_connection", "structcy__ota__cb__struct__t.html#a794d86b08cc8bcf1a3e509433795eac8", null ],
      [ "file", "structcy__ota__cb__struct__t.html#a71bfd9cf787fc18dfd42b54950103a25", null ],
      [ "unique_topic", "structcy__ota__cb__struct__t.html#ab3b3a87b636c188c6549c614a50ba4e3", null ],
      [ "json_doc", "structcy__ota__cb__struct__t.html#a2b43a418b275e5ef22173df42f4b9efa", null ]
    ] ],
    [ "cy_ota_network_params_t", "structcy__ota__network__params__t.html", [
      [ "initial_connection", "structcy__ota__network__params__t.html#abcc290be8049d49f332165ac7aafb3e0", null ],
      [ "mqtt", "structcy__ota__network__params__t.html#a6fa4db81577b20f350b43153adad5c9e", null ],
      [ "http", "structcy__ota__network__params__t.html#adeff13d7efd88ae4d641bcd3b33a37d9", null ],
      [ "use_get_job_flow", "structcy__ota__network__params__t.html#a5668e310a824240b5b756b23f63095c0", null ]
    ] ],
    [ "cy_ota_agent_params_t", "structcy__ota__agent__params__t.html", [
      [ "reboot_upon_completion", "structcy__ota__agent__params__t.html#adf754cf9aaa07e786e366dc2204d6429", null ],
      [ "validate_after_reboot", "structcy__ota__agent__params__t.html#a4474305852ba2b1f29e8b0d15ca671c5", null ],
      [ "do_not_send_result", "structcy__ota__agent__params__t.html#a05406d6107cc20e7cf9cde995ad3b33d", null ],
      [ "cb_func", "structcy__ota__agent__params__t.html#a2f3b68d908f64e031aca84d871554850", null ],
      [ "cb_arg", "structcy__ota__agent__params__t.html#a8734130b240a3c0121a412c1a92afffa", null ]
    ] ]
];